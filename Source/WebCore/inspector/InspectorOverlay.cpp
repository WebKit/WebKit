/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSStyleDeclaration.h"
#include "DOMCSSNamespace.h"
#include "DOMTokenList.h"
#include "Element.h"
#include "FloatLine.h"
#include "FloatPoint.h"
#include "FloatRoundedRect.h"
#include "FloatSize.h"
#include "FontCascade.h"
#include "FontCascadeDescription.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GridArea.h"
#include "GridPositionsResolver.h"
#include "InspectorClient.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Node.h"
#include "NodeList.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PseudoElement.h"
#include "RenderBox.h"
#include "RenderBoxModelObject.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderObject.h"
#include "Settings.h"
#include "StyleGridData.h"
#include "StyleResolver.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace Inspector;

static constexpr float elementDataSpacing = 2;
static constexpr float elementDataArrowSize = 7;
static constexpr float elementDataBorderSize = 1;

static constexpr float rulerSize = 15;
static constexpr float rulerLabelSize = 13;
static constexpr float rulerStepIncrement = 50;
static constexpr float rulerStepLength = 8;
static constexpr float rulerSubStepIncrement = 5;
static constexpr float rulerSubStepLength = 5;

static constexpr float layoutLabelPadding = 4;
static constexpr float layoutLabelArrowSize = 6;

static constexpr UChar bullet = 0x2022;
static constexpr UChar ellipsis = 0x2026;
static constexpr UChar multiplicationSign = 0x00D7;
static constexpr UChar thinSpace = 0x2009;
static constexpr UChar emSpace = 0x2003;

static void truncateWithEllipsis(String& string, size_t length)
{
    if (string.length() > length) {
        string.truncate(length);
        string.append(ellipsis);
    }
}

static FloatPoint localPointToRootPoint(const FrameView* view, const FloatPoint& point)
{
    return view->contentsToRootView(point);
}

static void contentsQuadToCoordinateSystem(const FrameView* mainView, const FrameView* view, FloatQuad& quad, InspectorOverlay::CoordinateSystem coordinateSystem)
{
    quad.setP1(localPointToRootPoint(view, quad.p1()));
    quad.setP2(localPointToRootPoint(view, quad.p2()));
    quad.setP3(localPointToRootPoint(view, quad.p3()));
    quad.setP4(localPointToRootPoint(view, quad.p4()));

    if (coordinateSystem == InspectorOverlay::CoordinateSystem::View)
        quad += toIntSize(mainView->scrollPosition());
}

static Element* effectiveElementForNode(Node& node)
{
    if (!is<Element>(node) || !node.document().frame())
        return nullptr;

    Element* element = nullptr;
    if (is<PseudoElement>(node)) {
        if (Element* hostElement = downcast<PseudoElement>(node).hostElement())
            element = hostElement;
    } else
        element = &downcast<Element>(node);

    return element;
}

static void buildRendererHighlight(RenderObject* renderer, const InspectorOverlay::Highlight::Config& highlightConfig, InspectorOverlay::Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem)
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
        highlight.type = InspectorOverlay::Highlight::Type::Rects;
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

            // RenderInline's bounding box includes padding and borders, excludes margins.
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

        highlight.type = InspectorOverlay::Highlight::Type::Node;
        highlight.quads.append(absMarginQuad);
        highlight.quads.append(absBorderQuad);
        highlight.quads.append(absPaddingQuad);
        highlight.quads.append(absContentQuad);
    }
}

static void buildNodeHighlight(Node& node, const InspectorOverlay::Highlight::Config& highlightConfig, InspectorOverlay::Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem)
{
    RenderObject* renderer = node.renderer();
    if (!renderer)
        return;

    buildRendererHighlight(renderer, highlightConfig, highlight, coordinateSystem);
}

static void buildQuadHighlight(const FloatQuad& quad, const InspectorOverlay::Highlight::Config& highlightConfig, InspectorOverlay::Highlight& highlight)
{
    highlight.setDataFromConfig(highlightConfig);
    highlight.type = InspectorOverlay::Highlight::Type::Rects;
    highlight.quads.append(quad);
}

static Path quadToPath(const FloatQuad& quad)
{
    Path path;
    path.moveTo(quad.p1());
    path.addLineTo(quad.p2());
    path.addLineTo(quad.p3());
    path.addLineTo(quad.p4());
    path.closeSubpath();

    return path;
}

static Path quadToPath(const FloatQuad& quad, InspectorOverlay::Highlight::Bounds& bounds)
{
    auto path = quadToPath(quad);
    bounds.unite(path.boundingRect());

    return path;
}

static void drawOutlinedQuadWithClip(GraphicsContext& context, const FloatQuad& quad, const FloatQuad& clipQuad, const Color& fillColor, InspectorOverlay::Highlight::Bounds& bounds)
{
    GraphicsContextStateSaver stateSaver(context);

    context.setFillColor(fillColor);
    context.setStrokeThickness(0);
    context.fillPath(quadToPath(quad, bounds));

    context.setCompositeOperation(CompositeOperator::DestinationOut);
    context.setFillColor(Color::red);
    context.fillPath(quadToPath(clipQuad, bounds));
}

static void drawOutlinedQuad(GraphicsContext& context, const FloatQuad& quad, const Color& fillColor, const Color& outlineColor, InspectorOverlay::Highlight::Bounds& bounds)
{
    Path path = quadToPath(quad, bounds);

    GraphicsContextStateSaver stateSaver(context);

    context.setStrokeThickness(2);

    context.clipPath(path);

    context.setFillColor(fillColor);
    context.fillPath(path);

    context.setStrokeColor(outlineColor);
    context.strokePath(path);
}

static void drawFragmentHighlight(GraphicsContext& context, Node& node, const InspectorOverlay::Highlight::Config& highlightConfig, InspectorOverlay::Highlight::Bounds& bounds)
{
    InspectorOverlay::Highlight highlight;
    buildNodeHighlight(node, highlightConfig, highlight, InspectorOverlay::CoordinateSystem::Document);

    FloatQuad marginQuad;
    FloatQuad borderQuad;
    FloatQuad paddingQuad;
    FloatQuad contentQuad;

    size_t size = highlight.quads.size();
    if (size >= 1)
        marginQuad = highlight.quads[0];
    if (size >= 2)
        borderQuad = highlight.quads[1];
    if (size >= 3)
        paddingQuad = highlight.quads[2];
    if (size >= 4)
        contentQuad = highlight.quads[3];

    if (!marginQuad.boundingBoxIsEmpty() && marginQuad != borderQuad && highlight.marginColor.isVisible())
        drawOutlinedQuadWithClip(context, marginQuad, borderQuad, highlight.marginColor, bounds);

    if (!borderQuad.boundingBoxIsEmpty() && borderQuad != paddingQuad && highlight.borderColor.isVisible())
        drawOutlinedQuadWithClip(context, borderQuad, paddingQuad, highlight.borderColor, bounds);

    if (!paddingQuad.boundingBoxIsEmpty() && paddingQuad != contentQuad && highlight.paddingColor.isVisible())
        drawOutlinedQuadWithClip(context, paddingQuad, contentQuad, highlight.paddingColor, bounds);

    if (!contentQuad.boundingBoxIsEmpty() && (highlight.contentColor.isVisible() || highlight.contentOutlineColor.isVisible()))
        drawOutlinedQuad(context, contentQuad, highlight.contentColor, highlight.contentOutlineColor, bounds);
}

static void drawShapeHighlight(GraphicsContext& context, Node& node, InspectorOverlay::Highlight::Bounds& bounds)
{
    RenderObject* renderer = node.renderer();
    if (!renderer || !is<RenderBox>(renderer))
        return;

    const ShapeOutsideInfo* shapeOutsideInfo = downcast<RenderBox>(renderer)->shapeOutsideInfo();
    if (!shapeOutsideInfo)
        return;

    Frame* containingFrame = node.document().frame();
    if (!containingFrame)
        return;

    FrameView* containingView = containingFrame->view();
    FrameView* mainView = containingFrame->page()->mainFrame().view();

    static constexpr auto shapeHighlightColor = SRGBA<uint8_t> { 96, 82, 127, 204 };

    Shape::DisplayPaths paths;
    shapeOutsideInfo->computedShape().buildDisplayPaths(paths);

    if (paths.shape.isEmpty()) {
        LayoutRect shapeBounds = shapeOutsideInfo->computedShapePhysicalBoundingBox();
        FloatQuad shapeQuad = renderer->localToAbsoluteQuad(FloatRect(shapeBounds));
        contentsQuadToCoordinateSystem(mainView, containingView, shapeQuad, InspectorOverlay::CoordinateSystem::Document);
        drawOutlinedQuad(context, shapeQuad, shapeHighlightColor, Color::transparentBlack, bounds);
        return;
    }

    const auto mapPoints = [&] (const Path& path) {
        Path newPath;
        path.apply([&] (const PathElement& pathElement) {
            const auto localToRoot = [&] (size_t index) {
                const FloatPoint& point = pathElement.points[index];
                return localPointToRootPoint(containingView, renderer->localToAbsolute(shapeOutsideInfo->shapeToRendererPoint(point)));
            };

            switch (pathElement.type) {
            case PathElement::Type::MoveToPoint:
                newPath.moveTo(localToRoot(0));
                break;

            case PathElement::Type::AddLineToPoint:
                newPath.addLineTo(localToRoot(0));
                break;

            case PathElement::Type::AddCurveToPoint:
                newPath.addBezierCurveTo(localToRoot(0), localToRoot(1), localToRoot(2));
                break;

            case PathElement::Type::AddQuadCurveToPoint:
                newPath.addQuadCurveTo(localToRoot(0), localToRoot(1));
                break;

            case PathElement::Type::CloseSubpath:
                newPath.closeSubpath();
                break;
            }
        });
        return newPath;
    };

    if (paths.marginShape.length()) {
        Path marginPath = mapPoints(paths.marginShape);
        bounds.unite(marginPath.boundingRect());

        GraphicsContextStateSaver stateSaver(context);

        constexpr auto shapeMarginHighlightColor = SRGBA<uint8_t> { 96, 82, 127, 153 };
        context.setFillColor(shapeMarginHighlightColor);
        context.fillPath(marginPath);
    }

    Path shapePath = mapPoints(paths.shape);
    bounds.unite(shapePath.boundingRect());

    GraphicsContextStateSaver stateSaver(context);

    context.setFillColor(shapeHighlightColor);
    context.fillPath(shapePath);
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

    FloatSize viewportSize = m_page.mainFrame().view()->sizeForVisibleContent();

    context.clearRect({ FloatPoint::zero(), viewportSize });

    GraphicsContextStateSaver stateSaver(context);

    if (m_indicating) {
        GraphicsContextStateSaver stateSaver(context);

        constexpr auto indicatingColor = SRGBA<uint8_t> { 111, 168, 220, 168 };
        context.setFillColor(indicatingColor);
        context.fillRect({ FloatPoint::zero(), viewportSize });
    }

    RulerExclusion rulerExclusion;

    if (m_highlightQuad) {
        auto quadRulerExclusion = drawQuadHighlight(context, *m_highlightQuad);
        rulerExclusion.bounds.unite(quadRulerExclusion.bounds);
    }

    if (m_highlightNodeList) {
        for (unsigned i = 0; i < m_highlightNodeList->length(); ++i) {
            if (auto* node = m_highlightNodeList->item(i)) {
                auto nodeRulerExclusion = drawNodeHighlight(context, *node);
                rulerExclusion.bounds.unite(nodeRulerExclusion.bounds);
            }
        }
    }

    if (m_highlightNode) {
        auto nodeRulerExclusion = drawNodeHighlight(context, *m_highlightNode);
        rulerExclusion.bounds.unite(nodeRulerExclusion.bounds);
        rulerExclusion.titlePath = nodeRulerExclusion.titlePath;
    }

    for (const InspectorOverlay::Grid& gridOverlay : m_activeGridOverlays) {
        if (auto gridHighlightOverlay = buildGridOverlay(gridOverlay))
            drawGridOverlay(context, *gridHighlightOverlay);
    }

    if (!m_paintRects.isEmpty())
        drawPaintRects(context, m_paintRects);

    if (m_showRulers || m_showRulersDuringElementSelection)
        drawRulers(context, rulerExclusion);
}

void InspectorOverlay::getHighlight(InspectorOverlay::Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem)
{
    if (!m_highlightNode && !m_highlightQuad && !m_highlightNodeList && !m_activeGridOverlays.size())
        return;

    highlight.type = InspectorOverlay::Highlight::Type::None;
    if (m_highlightNode)
        buildNodeHighlight(*m_highlightNode, m_nodeHighlightConfig, highlight, coordinateSystem);
    else if (m_highlightNodeList) {
        highlight.setDataFromConfig(m_nodeHighlightConfig);
        for (unsigned i = 0; i < m_highlightNodeList->length(); ++i) {
            InspectorOverlay::Highlight nodeHighlight;
            buildNodeHighlight(*(m_highlightNodeList->item(i)), m_nodeHighlightConfig, nodeHighlight, coordinateSystem);
            if (nodeHighlight.type == InspectorOverlay::Highlight::Type::Node)
                highlight.quads.appendVector(nodeHighlight.quads);
        }
        highlight.type = InspectorOverlay::Highlight::Type::NodeList;
    } else if (m_highlightQuad) {
        highlight.type = InspectorOverlay::Highlight::Type::Rects;
        buildQuadHighlight(*m_highlightQuad, m_quadHighlightConfig, highlight);
    }
    constexpr bool offsetBoundsByScroll = true;
    for (const InspectorOverlay::Grid& gridOverlay : m_activeGridOverlays) {
        if (auto gridHighlightOverlay = buildGridOverlay(gridOverlay, offsetBoundsByScroll))
            highlight.gridHighlightOverlays.append(*gridHighlightOverlay);
    }
}

void InspectorOverlay::hideHighlight()
{
    m_highlightNode = nullptr;
    m_highlightNodeList = nullptr;
    m_highlightQuad = nullptr;
    update();
}

void InspectorOverlay::highlightNodeList(RefPtr<NodeList>&& nodes, const InspectorOverlay::Highlight::Config& highlightConfig)
{
    m_nodeHighlightConfig = highlightConfig;
    m_highlightNodeList = WTFMove(nodes);
    m_highlightNode = nullptr;
    update();
}

void InspectorOverlay::highlightNode(Node* node, const InspectorOverlay::Highlight::Config& highlightConfig)
{
    m_nodeHighlightConfig = highlightConfig;
    m_highlightNode = node;
    m_highlightNodeList = nullptr;
    update();
}

void InspectorOverlay::highlightQuad(std::unique_ptr<FloatQuad> quad, const InspectorOverlay::Highlight::Config& highlightConfig)
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
    if (m_indicating == indicating)
        return;

    m_indicating = indicating;

    update();
}

bool InspectorOverlay::shouldShowOverlay() const
{
    // Don't show the overlay when m_showRulersDuringElementSelection is true, as it's only supposed
    // to have an effect when element selection is active (e.g. a node is hovered).
    return m_highlightNode || m_highlightNodeList || m_highlightQuad || m_indicating || m_showPaintRects || m_showRulers || m_activeGridOverlays.size();
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

    m_client->highlight();
}

void InspectorOverlay::setShowPaintRects(bool showPaintRects)
{
    if (m_showPaintRects == showPaintRects)
        return;

    m_showPaintRects = showPaintRects;
    if (!m_showPaintRects) {
        m_paintRects.clear();
        m_paintRectUpdateTimer.stop();
        update();
    }
}

void InspectorOverlay::showPaintRect(const FloatRect& rect)
{
    if (!m_showPaintRects)
        return;

    IntRect rootRect = m_page.mainFrame().view()->contentsToRootView(enclosingIntRect(rect));

    const auto removeDelay = 250_ms;

    MonotonicTime removeTime = MonotonicTime::now() + removeDelay;
    m_paintRects.append(TimeRectPair(removeTime, rootRect));

    if (!m_paintRectUpdateTimer.isActive()) {
        const Seconds paintRectsUpdateInterval { 32_ms };
        m_paintRectUpdateTimer.startRepeating(paintRectsUpdateInterval);
    }

    update();
}

void InspectorOverlay::setShowRulers(bool showRulers)
{
    if (m_showRulers == showRulers)
        return;

    m_showRulers = showRulers;

    update();
}

bool InspectorOverlay::removeGridOverlayForNode(Node& node)
{
    // Try to remove `node`. Also clear any grid overlays whose WeakPtr<Node> has been cleared.
    return m_activeGridOverlays.removeAllMatching([&] (const InspectorOverlay::Grid& gridOverlay) {
        return !gridOverlay.gridNode || gridOverlay.gridNode.get() == &node;
    });
}

ErrorStringOr<void> InspectorOverlay::setGridOverlayForNode(Node& node, const InspectorOverlay::Grid::Config& gridOverlayConfig)
{
    RenderObject* renderer = node.renderer();
    if (!is<RenderGrid>(renderer))
        return makeUnexpected("Node does not initiate a grid context");

    removeGridOverlayForNode(node);

    m_activeGridOverlays.append({ makeWeakPtr(node), gridOverlayConfig });

    update();

    return { };
}

ErrorStringOr<void> InspectorOverlay::clearGridOverlayForNode(Node& node)
{
    if (!removeGridOverlayForNode(node))
        return makeUnexpected("No grid overlay exists for the node, so cannot clear.");

    update();

    return { };
}

void InspectorOverlay::clearAllGridOverlays()
{
    m_activeGridOverlays.clear();

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

    if (rectsChanged)
        update();
}

InspectorOverlay::RulerExclusion InspectorOverlay::drawNodeHighlight(GraphicsContext& context, Node& node)
{
    RulerExclusion rulerExclusion;

    drawFragmentHighlight(context, node, m_nodeHighlightConfig, rulerExclusion.bounds);

    if (m_nodeHighlightConfig.showInfo)
        drawShapeHighlight(context, node, rulerExclusion.bounds);

    if (m_showRulers || m_showRulersDuringElementSelection)
        drawBounds(context, rulerExclusion.bounds);

    // Ensure that the title information is drawn after the bounds.
    if (m_nodeHighlightConfig.showInfo)
        rulerExclusion.titlePath = drawElementTitle(context, node, rulerExclusion.bounds);

    // Note: since grid overlays may cover the entire viewport with little lines, grid overlay bounds
    // are not considered as part of the combined bounds used as the ruler exclusion area.
    
    return rulerExclusion;
}

InspectorOverlay::RulerExclusion InspectorOverlay::drawQuadHighlight(GraphicsContext& context, const FloatQuad& quad)
{
    RulerExclusion rulerExclusion;

    InspectorOverlay::Highlight highlight;
    buildQuadHighlight(quad, m_quadHighlightConfig, highlight);

    if (highlight.quads.size() >= 1) {
        drawOutlinedQuad(context, highlight.quads[0], highlight.contentColor, highlight.contentOutlineColor, rulerExclusion.bounds);

        if (m_showRulers || m_showRulersDuringElementSelection)
            drawBounds(context, rulerExclusion.bounds);
    }

    return rulerExclusion;
}

void InspectorOverlay::drawPaintRects(GraphicsContext& context, const Deque<TimeRectPair>& paintRects)
{
    GraphicsContextStateSaver stateSaver(context);

    constexpr auto paintRectsColor = Color::red.colorWithAlphaByte(128);
    context.setFillColor(paintRectsColor);

    for (const TimeRectPair& pair : paintRects)
        context.fillRect(pair.second);
}

void InspectorOverlay::drawBounds(GraphicsContext& context, const InspectorOverlay::Highlight::Bounds& bounds)
{
    FrameView* pageView = m_page.mainFrame().view();
    FloatSize viewportSize = pageView->sizeForVisibleContent();
    FloatSize contentInset(0, pageView->topContentInset(ScrollView::TopContentInsetType::WebCoreOrPlatformContentInset));

    Path path;

    if (bounds.y() > contentInset.height()) {
        path.moveTo({ bounds.x(), bounds.y() });
        path.addLineTo({ bounds.x(), contentInset.height() });

        path.moveTo({ bounds.maxX(), bounds.y() });
        path.addLineTo({ bounds.maxX(), contentInset.height() });
    }

    if (bounds.maxY() < viewportSize.height()) {
        path.moveTo({ bounds.x(), viewportSize.height() });
        path.addLineTo({ bounds.x(), bounds.maxY() });

        path.moveTo({ bounds.maxX(), viewportSize.height() });
        path.addLineTo({ bounds.maxX(), bounds.maxY() });
    }

    if (bounds.x() > contentInset.width()) {
        path.moveTo({ bounds.x(), bounds.y() });
        path.addLineTo({ contentInset.width(), bounds.y() });

        path.moveTo({ bounds.x(), bounds.maxY() });
        path.addLineTo({ contentInset.width(), bounds.maxY() });
    }

    if (bounds.maxX() < viewportSize.width()) {
        path.moveTo({ bounds.maxX(), bounds.y() });
        path.addLineTo({ viewportSize.width(), bounds.y() });

        path.moveTo({ bounds.maxX(), bounds.maxY() });
        path.addLineTo({ viewportSize.width(), bounds.maxY() });
    }

    GraphicsContextStateSaver stateSaver(context);

    context.setStrokeThickness(1);

    constexpr auto boundsColor = Color::red.colorWithAlphaByte(153);
    context.setStrokeColor(boundsColor);

    context.strokePath(path);
}

void InspectorOverlay::drawRulers(GraphicsContext& context, const InspectorOverlay::RulerExclusion& rulerExclusion)
{
    constexpr auto rulerBackgroundColor = Color::white.colorWithAlphaByte(153);
    constexpr auto lightRulerColor = Color::black.colorWithAlphaByte(51);
    constexpr auto darkRulerColor = Color::black.colorWithAlphaByte(128);

    IntPoint scrollOffset;

    FrameView* pageView = m_page.mainFrame().view();
    if (!pageView->delegatesScrolling())
        scrollOffset = pageView->visibleContentRect().location();

    FloatSize viewportSize = pageView->sizeForVisibleContent();
    FloatSize contentInset(0, pageView->topContentInset(ScrollView::TopContentInsetType::WebCoreOrPlatformContentInset));
    float pageScaleFactor = m_page.pageScaleFactor();
    float pageZoomFactor = m_page.mainFrame().pageZoomFactor();

    float pageFactor = pageZoomFactor * pageScaleFactor;
    float scrollX = scrollOffset.x() * pageScaleFactor;
    float scrollY = scrollOffset.y() * pageScaleFactor;

    const auto zoom = [&] (float value) -> float {
        return value * pageFactor;
    };

    const auto unzoom = [&] (float value) -> float {
        return value / pageFactor;
    };

    const auto multipleBelow = [&] (float value, float step) -> float {
        return value - std::fmod(value, step);
    };

    float width = viewportSize.width() / pageFactor;
    float height = viewportSize.height() / pageFactor;
    float minX = unzoom(scrollX);
    float minY = unzoom(scrollY);
    float maxX = minX + width;
    float maxY = minY + height;

    bool drawTopEdge = true;
    bool drawLeftEdge = true;

    // Determine which side (top/bottom and left/right) to draw the rulers.
    {
        FloatRect topEdge(contentInset.width(), contentInset.height(), zoom(width) - contentInset.width(), rulerSize);
        FloatRect bottomEdge(contentInset.width(), zoom(height) - rulerSize, zoom(width) - contentInset.width(), rulerSize);
        drawTopEdge = !rulerExclusion.bounds.intersects(topEdge) || rulerExclusion.bounds.intersects(bottomEdge);

        FloatRect rightEdge(zoom(width) - rulerSize, contentInset.height(), rulerSize, zoom(height) - contentInset.height());
        FloatRect leftEdge(contentInset.width(), contentInset.height(), rulerSize, zoom(height) - contentInset.height());
        drawLeftEdge = !rulerExclusion.bounds.intersects(leftEdge) || rulerExclusion.bounds.intersects(rightEdge);
    }

    float cornerX = drawLeftEdge ? contentInset.width() : zoom(width) - rulerSize;
    float cornerY = drawTopEdge ? contentInset.height() : zoom(height) - rulerSize;

    // Draw backgrounds.
    {
        GraphicsContextStateSaver backgroundStateSaver(context);

        context.setFillColor(rulerBackgroundColor);

        context.fillRect({ cornerX, cornerY, rulerSize, rulerSize });

        if (drawLeftEdge)
            context.fillRect({ cornerX + rulerSize, cornerY, zoom(width) - cornerX - rulerSize, rulerSize });
        else
            context.fillRect({ contentInset.width(), cornerY, cornerX - contentInset.width(), rulerSize });

        if (drawTopEdge)
            context.fillRect({ cornerX, cornerY + rulerSize, rulerSize, zoom(height) - cornerY - rulerSize });  
        else
            context.fillRect({ cornerX, contentInset.height(), rulerSize, cornerY - contentInset.height() });
    }

    // Draw lines.
    {
        FontCascadeDescription fontDescription;
        fontDescription.setOneFamily(m_page.settings().sansSerifFontFamily());
        fontDescription.setComputedSize(10);

        FontCascade font(WTFMove(fontDescription), 0, 0);
        font.update(nullptr);

        GraphicsContextStateSaver lineStateSaver(context);

        context.setFillColor(darkRulerColor);
        context.setStrokeThickness(1);

        // Draw horizontal ruler.
        {
            GraphicsContextStateSaver horizontalRulerStateSaver(context);

            context.translate(contentInset.width() - scrollX + 0.5f, cornerY - scrollY);

            for (float x = multipleBelow(minX, rulerSubStepIncrement); x < maxX; x += rulerSubStepIncrement) {
                if (!x && !scrollX)
                    continue;

                Path path;
                path.moveTo({ zoom(x), drawTopEdge ? scrollY : scrollY + rulerSize });

                float lineLength = 0.0f;
                if (std::fmod(x, rulerStepIncrement)) {
                    lineLength = rulerSubStepLength;
                    context.setStrokeColor(lightRulerColor);
                } else {
                    lineLength = std::fmod(x, rulerStepIncrement * 2) ? rulerSubStepLength : rulerStepLength;
                    context.setStrokeColor(darkRulerColor);
                }
                path.addLineTo({ zoom(x), scrollY + (drawTopEdge ? lineLength : rulerSize - lineLength) });

                context.strokePath(path);
            }

            // Draw labels.
            for (float x = multipleBelow(minX, rulerStepIncrement * 2); x < maxX; x += rulerStepIncrement * 2) {
                if (!x && !scrollX)
                    continue;

                GraphicsContextStateSaver verticalLabelStateSaver(context);
                context.translate(zoom(x) + 0.5f, scrollY);
                context.drawText(font, TextRun(String::number(x)), { 2, drawTopEdge ? rulerLabelSize : rulerLabelSize - rulerSize + font.fontMetrics().height() - 1.0f });
            }
        }

        // Draw vertical ruler.
        {
            GraphicsContextStateSaver veritcalRulerStateSaver(context);

            context.translate(cornerX - scrollX, contentInset.height() - scrollY + 0.5f);

            for (float y = multipleBelow(minY, rulerSubStepIncrement); y < maxY; y += rulerSubStepIncrement) {
                if (!y && !scrollY)
                    continue;

                Path path;
                path.moveTo({ drawLeftEdge ? scrollX : scrollX + rulerSize, zoom(y) });

                float lineLength = 0.0f;
                if (std::fmod(y, rulerStepIncrement)) {
                    lineLength = rulerSubStepLength;
                    context.setStrokeColor(lightRulerColor);
                } else {
                    lineLength = std::fmod(y, rulerStepIncrement * 2) ? rulerSubStepLength : rulerStepLength;
                    context.setStrokeColor(darkRulerColor);
                }
                path.addLineTo({ scrollX + (drawLeftEdge ? lineLength : rulerSize - lineLength), zoom(y) });

                context.strokePath(path);
            }

            // Draw labels.
            for (float y = multipleBelow(minY, rulerStepIncrement * 2); y < maxY; y += rulerStepIncrement * 2) {
                if (!y && !scrollY)
                    continue;

                GraphicsContextStateSaver horizontalLabelStateSaver(context);
                context.translate(scrollX, zoom(y) + 0.5f);
                context.rotate(drawLeftEdge ? -piOverTwoFloat : piOverTwoFloat);
                context.drawText(font, TextRun(String::number(y)), { 2, drawLeftEdge ? rulerLabelSize : rulerLabelSize - rulerSize });
            }
        }
    }

    // Draw viewport size.
    {
        FontCascadeDescription fontDescription;
        fontDescription.setOneFamily(m_page.settings().sansSerifFontFamily());
        fontDescription.setComputedSize(12);

        FontCascade font(WTFMove(fontDescription), 0, 0);
        font.update(nullptr);

        auto viewportRect = pageView->visualViewportRect();
        TextRun viewportTextRun(makeString(viewportRect.width() / pageZoomFactor, "px", ' ', multiplicationSign, ' ', viewportRect.height() / pageZoomFactor, "px"));

        const float margin = 4;
        const float padding = 2;
        const float radius = 4;
        float fontWidth = font.width(viewportTextRun);
        float fontHeight = font.fontMetrics().floatHeight();
        FloatRect viewportTextRect(margin, margin, (padding * 2.0f) + fontWidth, (padding * 2.0f) + fontHeight);
        const auto viewportTextRectCenter = viewportTextRect.center();

        GraphicsContextStateSaver viewportSizeStateSaver(context);

        float leftTranslateX = rulerSize;
        float rightTranslateX = 0.0f - (margin * 2.0f) - (padding * 2.0f) - fontWidth;
        float translateX = cornerX + (drawLeftEdge ? leftTranslateX : rightTranslateX);

        float topTranslateY = rulerSize;
        float bottomTranslateY = 0.0f - (margin * 2.0f) - (padding * 2.0f) - fontHeight;
        float translateY = cornerY + (drawTopEdge ? topTranslateY : bottomTranslateY);

        FloatPoint translate(translateX, translateY);
        if (rulerExclusion.titlePath.contains(viewportTextRectCenter + translate)) {
            // Try the opposite horizontal side.
            float oppositeTranslateX = drawLeftEdge ? zoom(width) + rightTranslateX : contentInset.width() + leftTranslateX;
            translate.setX(oppositeTranslateX);

            if (rulerExclusion.titlePath.contains(viewportTextRectCenter + translate)) {
                translate.setX(translateX);

                // Try the opposite vertical side.
                float oppositeTranslateY = drawTopEdge ? zoom(height) + bottomTranslateY : contentInset.height() + topTranslateY;
                translate.setY(oppositeTranslateY);

                if (rulerExclusion.titlePath.contains(viewportTextRectCenter + translate)) {
                    // Try the opposite corner.
                    translate.setX(oppositeTranslateX);
                }
            }
        }
        context.translate(translate);

        context.fillRoundedRect(FloatRoundedRect(viewportTextRect, FloatRoundedRect::Radii(radius)), rulerBackgroundColor);

        context.setFillColor(Color::black);
        context.drawText(font, viewportTextRun, {margin +  padding, margin + padding + fontHeight - font.fontMetrics().descent() });
    }
}

Path InspectorOverlay::drawElementTitle(GraphicsContext& context, Node& node, const InspectorOverlay::Highlight::Bounds& bounds)
{
    if (bounds.isEmpty())
        return { };

    Element* element = effectiveElementForNode(node);
    if (!element)
        return { };

    RenderObject* renderer = node.renderer();
    if (!renderer)
        return { };

    String elementTagName = element->nodeName();
    if (!element->document().isXHTMLDocument())
        elementTagName = elementTagName.convertToASCIILowercase();

    String elementIDValue;
    if (element->hasID())
        elementIDValue = makeString('#', DOMCSSNamespace::escape(element->getIdAttribute()));

    String elementClassValue;
    if (element->hasClass()) {
        StringBuilder builder;
        DOMTokenList& classList = element->classList();
        for (size_t i = 0; i < classList.length(); ++i) {
            builder.append('.');
            builder.append(DOMCSSNamespace::escape(classList.item(i)));
        }

        elementClassValue = builder.toString();
        truncateWithEllipsis(elementClassValue, 50);
    }

    String elementPseudoType;
    if (node.isBeforePseudoElement())
        elementPseudoType = "::before"_s;
    else if (node.isAfterPseudoElement())
        elementPseudoType = "::after"_s;

    String elementWidth;
    String elementHeight;
    if (is<RenderBoxModelObject>(renderer)) {
        RenderBoxModelObject* modelObject = downcast<RenderBoxModelObject>(renderer);
        elementWidth = String::number(adjustForAbsoluteZoom(roundToInt(modelObject->offsetWidth()), *modelObject));
        elementHeight = String::number(adjustForAbsoluteZoom(roundToInt(modelObject->offsetHeight()), *modelObject));
    } else {
        FrameView* containingView = node.document().frame()->view();
        IntRect boundingBox = snappedIntRect(containingView->contentsToRootView(renderer->absoluteBoundingBoxRect()));
        elementWidth = String::number(boundingBox.width());
        elementHeight = String::number(boundingBox.height());
    }

    // Need to enable AX to get the computed role.
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();

    String elementRole;
    if (AXObjectCache* axObjectCache = node.document().axObjectCache()) {
        if (AccessibilityObject* axObject = axObjectCache->getOrCreate(&node))
            elementRole = axObject->computedRoleString();
    }

    FontCascadeDescription fontDescription;
    fontDescription.setFamilies({ "Menlo", m_page.settings().fixedFontFamily() });
    fontDescription.setComputedSize(11);

    FontCascade font(WTFMove(fontDescription), 0, 0);
    font.update(nullptr);

    int fontHeight = font.fontMetrics().height();

    float elementDataWidth;
    float elementDataHeight = fontHeight;
    bool hasSecondLine = !elementRole.isEmpty();

    {
        auto firstLine = makeString(elementTagName, elementIDValue, elementClassValue, elementPseudoType, ' ', elementWidth, "px", ' ', multiplicationSign, ' ', elementHeight, "px");
        auto secondLine = makeString("Role ", elementRole);

        float firstLineWidth = font.width(TextRun(firstLine));
        float secondLineWidth = font.width(TextRun(secondLine));

        elementDataWidth = std::fmax(firstLineWidth, secondLineWidth);
        if (hasSecondLine)
            elementDataHeight += fontHeight;
    }

    FrameView* pageView = m_page.mainFrame().view();

    FloatSize viewportSize = pageView->sizeForVisibleContent();
    viewportSize.expand(-elementDataSpacing, -elementDataSpacing);

    FloatSize contentInset(0, pageView->topContentInset(ScrollView::TopContentInsetType::WebCoreOrPlatformContentInset));
    contentInset.expand(elementDataSpacing, elementDataSpacing);
    if (m_showRulers || m_showRulersDuringElementSelection)
        contentInset.expand(rulerSize, rulerSize);

    float anchorTop = bounds.y();
    float anchorBottom = bounds.maxY();

    bool renderArrowUp = false;
    bool renderArrowDown = false;

    float boxWidth = elementDataWidth + (elementDataSpacing * 2);
    float boxHeight = elementDataArrowSize + elementDataHeight + (elementDataSpacing * 2);

    float boxX = bounds.x();
    if (boxX < contentInset.width())
        boxX = contentInset.width();
    else if (boxX > viewportSize.width() - boxWidth)
        boxX = viewportSize.width() - boxWidth;
    else
        boxX += elementDataSpacing;

    float boxY;
    if (anchorTop > viewportSize.height()) {
        boxY = viewportSize.height() - boxHeight;
        renderArrowDown = true;
    } else if (anchorBottom < contentInset.height()) {
        boxY = contentInset.height() + elementDataArrowSize;
        renderArrowUp = true;
    } else if (anchorTop - boxHeight - elementDataSpacing > contentInset.height()) {
        boxY = anchorTop - boxHeight - elementDataSpacing;
        renderArrowDown = true;
    } else if (anchorBottom + boxHeight + elementDataSpacing < viewportSize.height()) {
        boxY = anchorBottom + elementDataArrowSize + elementDataSpacing;
        renderArrowUp = true;
    } else {
        boxY = contentInset.height();
        renderArrowDown = true;
    }

    Path path;
    path.moveTo({ boxX, boxY });
    if (renderArrowUp) {
        path.addLineTo({ boxX + (elementDataArrowSize * 2), boxY });
        path.addLineTo({ boxX + (elementDataArrowSize * 3), boxY - elementDataArrowSize });
        path.addLineTo({ boxX + (elementDataArrowSize * 4), boxY });
    }
    path.addLineTo({ boxX + elementDataWidth + (elementDataSpacing * 2), boxY });
    path.addLineTo({ boxX + elementDataWidth + (elementDataSpacing * 2), boxY + elementDataHeight + (elementDataSpacing * 2) });
    if (renderArrowDown) {
        path.addLineTo({ boxX + (elementDataArrowSize * 4), boxY + elementDataHeight + (elementDataSpacing * 2) });
        path.addLineTo({ boxX + (elementDataArrowSize * 3), boxY + elementDataHeight + (elementDataSpacing * 2) + elementDataArrowSize });
        path.addLineTo({ boxX + (elementDataArrowSize * 2), boxY + elementDataHeight + (elementDataSpacing * 2) });
    }
    path.addLineTo({ boxX, boxY + elementDataHeight + (elementDataSpacing * 2) });
    path.closeSubpath();

    GraphicsContextStateSaver stateSaver(context);

    context.translate(elementDataBorderSize / 2.0f, elementDataBorderSize / 2.0f);

    constexpr auto elementTitleBackgroundColor = SRGBA<uint8_t> { 255, 255, 194 };
    context.setFillColor(elementTitleBackgroundColor);

    context.fillPath(path);

    context.setStrokeThickness(elementDataBorderSize);

    constexpr auto elementTitleBorderColor = Color::darkGray;
    context.setStrokeColor(elementTitleBorderColor);

    context.strokePath(path);

    FloatPoint textPosition(boxX + elementDataSpacing, boxY - (elementDataSpacing / 2.0f) + fontHeight);
    const auto drawText = [&] (const String& text, SRGBA<uint8_t> color) {
        if (text.isEmpty())
            return;

        context.setFillColor(color);
        textPosition += context.drawText(font, TextRun(text), textPosition);
    };

    drawText(elementTagName, { 136, 18, 128 }); // Keep this in sync with XMLViewer.css (.tag)
    drawText(elementIDValue, { 26, 26, 166 }); // Keep this in sync with XMLViewer.css (.attribute-value)
    drawText(elementClassValue, { 153, 69, 0 }); // Keep this in sync with XMLViewer.css (.attribute-name)
    drawText(elementPseudoType, { 136, 18, 128 }); // Keep this in sync with XMLViewer.css (.tag)
    drawText(" "_s, Color::black);
    drawText(elementWidth, Color::black);
    drawText("px"_s, Color::darkGray);
    drawText(" "_s, Color::darkGray);
    drawText(makeString(multiplicationSign), Color::darkGray);
    drawText(" "_s, Color::darkGray);
    drawText(elementHeight, Color::black);
    drawText("px"_s, Color::darkGray);

    if (hasSecondLine) {
        textPosition.setX(boxX + elementDataSpacing);
        textPosition.move(0, fontHeight);
        
        drawText("Role"_s, { 170, 13, 145 });
        drawText(" "_s, Color::black);
        drawText(elementRole, Color::black);
    }

    return path;
}

void InspectorOverlay::drawLayoutHatching(GraphicsContext& context, FloatQuad quad)
{
    GraphicsContextStateSaver saver(context);
    context.clipPath(quadToPath(quad));
    context.setStrokeThickness(0.5);
    context.setStrokeStyle(StrokeStyle::DashedStroke);
    context.setLineDash({ 2, 2 }, 2);
        
    constexpr auto hatchSpacing = 12;
    Path hatchPath;
    
    FloatLine topSide = { quad.p1(), quad.p2() };
    FloatLine leftSide = { quad.p1(), quad.p4() };
    
    // The opposite axis' length is used to determine how far to draw a hatch line in both dimensions, which keeps the lines at a 45deg angle.
    if (topSide.length() > leftSide.length()) {
        FloatLine bottomSide = { quad.p4(), quad.p3() };
        // Move across the relative top of the quad, starting left of `0, 0` to ensure that the tail of the previous hatch line is drawn while scrolling.
        for (float x = -leftSide.length(); x < topSide.length(); x += hatchSpacing) {
            hatchPath.moveTo(topSide.pointAtAbsoluteDistance(x));
            hatchPath.addLineTo(bottomSide.pointAtAbsoluteDistance(x + leftSide.length()));
        }
    } else {
        FloatLine rightSide = { quad.p2(), quad.p3() };
        // Move down the relative left side of the quad, starting above `0, 0` to ensure that the tail of the previous hatch line is drawn while scrolling.
        for (float y = -topSide.length(); y < leftSide.length(); y += hatchSpacing) {
            hatchPath.moveTo(leftSide.pointAtAbsoluteDistance(y));
            hatchPath.addLineTo(rightSide.pointAtAbsoluteDistance(y + topSide.length()));
        }
    }
    
    context.strokePath(hatchPath);
}

FontCascade InspectorOverlay::fontForLayoutLabel()
{
    FontCascadeDescription fontDescription;
    fontDescription.setFamilies({ "system-ui" });
    fontDescription.setWeight(FontSelectionValue(500));
    fontDescription.setComputedSize(12);

    FontCascade font(WTFMove(fontDescription), 0, 0);
    font.update(nullptr);
    return font;
}

Path InspectorOverlay::backgroundPathForLayoutLabel(float width, float height, InspectorOverlay::LabelArrowDirection arrowDirection, InspectorOverlay::LabelArrowEdgePosition arrowEdgePosition, float arrowSize)
{
    Path path;
    FloatSize offsetForArrowEdgePosition;

    switch (arrowDirection) {
    case InspectorOverlay::LabelArrowDirection::Down:
        path.moveTo({ -(width / 2), -height - arrowSize});
        path.addLineTo({ -(width / 2), -arrowSize });

        switch (arrowEdgePosition) {
        case InspectorOverlay::LabelArrowEdgePosition::Leading:
            path.addLineTo({ -(width / 2), 0 });
            path.addLineTo({ -(width / 2) + arrowSize, -arrowSize });
            offsetForArrowEdgePosition = { (width / 2), 0 };
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Middle:
            path.addLineTo({ -arrowSize, -arrowSize });
            path.addLineTo({ 0, 0 });
            path.addLineTo({ arrowSize, -arrowSize });
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Trailing:
            path.addLineTo({ (width / 2) - arrowSize, -arrowSize });
            path.addLineTo({ (width / 2), 0 });
            offsetForArrowEdgePosition = { -(width / 2), 0 };
            break;
        case InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }

        path.addLineTo({ (width / 2), -arrowSize });
        path.addLineTo({ (width / 2), -height - arrowSize });
        break;
    case InspectorOverlay::LabelArrowDirection::Up:
        path.moveTo({ -(width / 2), height + arrowSize });
        path.addLineTo({ -(width / 2), arrowSize });

        switch (arrowEdgePosition) {
        case InspectorOverlay::LabelArrowEdgePosition::Leading:
            path.addLineTo({ -(width / 2), 0 });
            path.addLineTo({ -(width / 2) + arrowSize, arrowSize });
            offsetForArrowEdgePosition = { (width / 2), 0 };
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Middle:
            path.addLineTo({ -arrowSize, arrowSize });
            path.addLineTo({ 0, 0 });
            path.addLineTo({ arrowSize, arrowSize });
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Trailing:
            path.addLineTo({ (width / 2) - arrowSize, arrowSize });
            path.addLineTo({ (width / 2), 0 });
            offsetForArrowEdgePosition = { -(width / 2), 0 };
            break;
        case InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }

        path.addLineTo({ (width / 2), arrowSize });
        path.addLineTo({ (width / 2), height + arrowSize });
        break;
    case InspectorOverlay::LabelArrowDirection::Right:
        path.moveTo({ -width - arrowSize, (height / 2) });
        path.addLineTo({ -arrowSize, (height / 2) });

        switch (arrowEdgePosition) {
        case InspectorOverlay::LabelArrowEdgePosition::Leading:
            path.addLineTo({ -arrowSize, -(height / 2) + arrowSize });
            path.addLineTo({ 0, -(height / 2) });
            offsetForArrowEdgePosition = { 0, (height / 2) };
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Middle:
            path.addLineTo({ -arrowSize, arrowSize });
            path.addLineTo({ 0, 0 });
            path.addLineTo({ -arrowSize, -arrowSize });
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Trailing:
            path.addLineTo({ 0, (height / 2) });
            path.addLineTo({ -arrowSize, (height / 2) - arrowSize });
            offsetForArrowEdgePosition = { 0, -(height / 2) };
            break;
        case InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }

        path.addLineTo({ -arrowSize, -(height / 2) });
        path.addLineTo({ -width - arrowSize, -(height / 2) });
        break;
    case InspectorOverlay::LabelArrowDirection::Left:
        path.moveTo({ width + arrowSize, (height / 2) });
        path.addLineTo({ arrowSize, (height / 2) });

        switch (arrowEdgePosition) {
        case InspectorOverlay::LabelArrowEdgePosition::Leading:
            path.addLineTo({ arrowSize, -(height / 2) + arrowSize });
            path.addLineTo({ 0, -(height / 2) });
            offsetForArrowEdgePosition = { 0, (height / 2) };
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Middle:
            path.addLineTo({ arrowSize, arrowSize });
            path.addLineTo({ 0, 0 });
            path.addLineTo({ arrowSize, -arrowSize });
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Trailing:
            path.addLineTo({ 0, (height / 2) });
            path.addLineTo({ arrowSize, (height / 2) - arrowSize });
            offsetForArrowEdgePosition = { 0, -(height / 2) };
            break;
        case InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }

        path.addLineTo({ arrowSize, -(height / 2) });
        path.addLineTo({ width + arrowSize, -(height / 2) });
        break;
    case InspectorOverlay::LabelArrowDirection::None:
        path.moveTo({ -(width / 2), -(height / 2) });
        path.addLineTo({ -(width / 2), height / 2 });
        path.addLineTo({ width / 2, height / 2 });
        path.addLineTo({ width / 2, -(height / 2) });
        break;
    }

    path.closeSubpath();
    path.translate(offsetForArrowEdgePosition);

    return path;
}

static FloatSize expectedSizeForLayoutLabel(String label, InspectorOverlay::LabelArrowDirection direction, float maximumWidth = 0)
{
    auto font = InspectorOverlay::fontForLayoutLabel();

    float textHeight = font.fontMetrics().floatHeight();
    float textWidth = font.width(TextRun(label));
    if (maximumWidth && textWidth + (layoutLabelPadding * 2) > maximumWidth)
        textWidth = maximumWidth;

    switch (direction) {
    case InspectorOverlay::LabelArrowDirection::Down:
    case InspectorOverlay::LabelArrowDirection::Up:
        return { textWidth + (layoutLabelPadding * 2), textHeight + (layoutLabelPadding * 2) + layoutLabelArrowSize };
    case InspectorOverlay::LabelArrowDirection::Right:
    case InspectorOverlay::LabelArrowDirection::Left:
        return { textWidth + (layoutLabelPadding * 2) + layoutLabelArrowSize, textHeight + (layoutLabelPadding * 2) };
    case InspectorOverlay::LabelArrowDirection::None:
        return { textWidth + (layoutLabelPadding * 2), textHeight + (layoutLabelPadding * 2) };
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void InspectorOverlay::drawLayoutLabel(GraphicsContext& context, String label, FloatPoint point, InspectorOverlay::LabelArrowDirection arrowDirection, InspectorOverlay::LabelArrowEdgePosition arrowEdgePosition, Color backgroundColor, float maximumWidth)
{
    ASSERT(arrowEdgePosition != LabelArrowEdgePosition::None || arrowDirection == LabelArrowDirection::None);

    GraphicsContextStateSaver saver(context);
    
    context.translate(point);

    auto font = fontForLayoutLabel();
    float textHeight = font.fontMetrics().floatHeight();
    float textDescent = font.fontMetrics().floatDescent();
    
    float textWidth = font.width(TextRun(label));
    if (maximumWidth && textWidth + (layoutLabelPadding * 2) > maximumWidth) {
        label.append("..."_s);
        while (textWidth + (layoutLabelPadding * 2) > maximumWidth && label.length() >= 4) {
            // Remove the fourth from last character (the character before the ellipsis) and remeasure.
            label.remove(label.length() - 4);
            textWidth = font.width(TextRun(label));
        }
    }

    FloatPoint textPosition;
    switch (arrowDirection) {
    case InspectorOverlay::LabelArrowDirection::Down:
        switch (arrowEdgePosition) {
        case InspectorOverlay::LabelArrowEdgePosition::Leading:
            textPosition = FloatPoint(layoutLabelPadding, -textDescent - layoutLabelArrowSize - layoutLabelPadding);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Middle:
            textPosition = FloatPoint(-(textWidth / 2), -textDescent - layoutLabelArrowSize - layoutLabelPadding);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Trailing:
            textPosition = FloatPoint(-(textWidth) - layoutLabelPadding, -textDescent - layoutLabelArrowSize - layoutLabelPadding);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }
        break;
    case InspectorOverlay::LabelArrowDirection::Up:
        switch (arrowEdgePosition) {
        case InspectorOverlay::LabelArrowEdgePosition::Leading:
            textPosition = FloatPoint(layoutLabelPadding, textHeight - textDescent + layoutLabelArrowSize + layoutLabelPadding);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Middle:
            textPosition = FloatPoint(-(textWidth / 2), textHeight - textDescent + layoutLabelArrowSize + layoutLabelPadding);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Trailing:
            textPosition = FloatPoint(-(textWidth) - layoutLabelPadding, textHeight - textDescent + layoutLabelArrowSize + layoutLabelPadding);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }
        break;
    case InspectorOverlay::LabelArrowDirection::Right:
        switch (arrowEdgePosition) {
        case InspectorOverlay::LabelArrowEdgePosition::Leading:
            textPosition = FloatPoint(-textWidth - layoutLabelArrowSize - layoutLabelPadding, layoutLabelPadding + textHeight - textDescent);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Middle:
            textPosition = FloatPoint(-textWidth - layoutLabelArrowSize - layoutLabelPadding, (textHeight / 2) - textDescent);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Trailing:
            textPosition = FloatPoint(-textWidth - layoutLabelArrowSize - layoutLabelPadding, -layoutLabelPadding - textDescent);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }
        break;
    case InspectorOverlay::LabelArrowDirection::Left:
        switch (arrowEdgePosition) {
        case InspectorOverlay::LabelArrowEdgePosition::Leading:
            textPosition = FloatPoint(layoutLabelArrowSize + layoutLabelPadding, layoutLabelPadding + textHeight - textDescent);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Middle:
            textPosition = FloatPoint(layoutLabelArrowSize + layoutLabelPadding, (textHeight / 2) - textDescent);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::Trailing:
            textPosition = FloatPoint(layoutLabelArrowSize + layoutLabelPadding, -layoutLabelPadding - textDescent);
            break;
        case InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }
        break;
    case InspectorOverlay::LabelArrowDirection::None:
        textPosition = FloatPoint(-(textWidth / 2), (textHeight / 2) - textDescent);
        break;
    }

    Path labelPath = backgroundPathForLayoutLabel(textWidth + (layoutLabelPadding * 2), textHeight + (layoutLabelPadding * 2), arrowDirection, arrowEdgePosition, layoutLabelArrowSize);

    context.setFillColor(backgroundColor);
    context.fillPath(labelPath);
    context.strokePath(labelPath);
    
    context.setFillColor(Color::black);
    context.drawText(font, TextRun(label), textPosition);
}

void InspectorOverlay::drawGridOverlay(GraphicsContext& context, const InspectorOverlay::Highlight::GridHighlightOverlay& gridOverlay)
{
    constexpr auto translucentLabelBackgroundColor = Color::white.colorWithAlphaByte(230);

    GraphicsContextStateSaver saver(context);
    context.setStrokeThickness(1);
    context.setStrokeColor(gridOverlay.color);

    Path gridLinesPath;
    for (auto gridLine : gridOverlay.gridLines) {
        gridLinesPath.moveTo(gridLine.start());
        gridLinesPath.addLineTo(gridLine.end());
    }
    context.strokePath(gridLinesPath);

    for (auto gapQuad : gridOverlay.gaps)
        drawLayoutHatching(context, gapQuad);

    context.setStrokeThickness(3);
    for (auto area : gridOverlay.areas)
        context.strokePath(quadToPath(area.quad));

    // Draw labels on top of all other lines.
    context.setStrokeThickness(1);
    for (auto area : gridOverlay.areas)
        drawLayoutLabel(context, area.name, area.quad.center(), LabelArrowDirection::None, LabelArrowEdgePosition::None, translucentLabelBackgroundColor, area.quad.boundingBox().width());

    for (auto label : gridOverlay.labels)
        drawLayoutLabel(context, label.text, label.location, label.arrowDirection, label.arrowEdgePosition, label.backgroundColor);
}

static Vector<String> authoredGridTrackSizes(Node* node, GridTrackSizingDirection direction, unsigned expectedTrackCount)
{
    if (!is<StyledElement>(node))
        return { };

    auto element = downcast<StyledElement>(node);
    auto directionCSSPropertyID = direction == GridTrackSizingDirection::ForColumns ? CSSPropertyID::CSSPropertyGridTemplateColumns : CSSPropertyID::CSSPropertyGridTemplateRows;
    RefPtr<CSSValue> cssValue = element->cssomStyle().getPropertyCSSValueInternal(directionCSSPropertyID);

    if (!cssValue) {
        auto styleRules = element->styleResolver().styleRulesForElement(element);
        styleRules.reverse();
        for (auto styleRule : styleRules) {
            ASSERT(styleRule);
            if (!styleRule)
                continue;
            cssValue = styleRule->properties().getPropertyCSSValue(directionCSSPropertyID);
            if (cssValue)
                break;
        }
    }

    if (!cssValue || !is<CSSValueList>(cssValue))
        return { };
    
    Vector<String> trackSizes;
    
    auto handleValueIgnoringLineNames = [&](const CSSValue& currentValue) {
        if (!is<CSSGridLineNamesValue>(currentValue))
            trackSizes.append(currentValue.cssText());
    };

    for (auto& currentValue : downcast<CSSValueList>(*cssValue)) {
        if (is<CSSGridAutoRepeatValue>(currentValue)) {
            // Auto-repeated values will be looped through until no more values were used in layout based on the expected track count.
            while (trackSizes.size() < expectedTrackCount) {
                for (auto& autoRepeatValue : downcast<CSSValueList>(currentValue.get())) {
                    handleValueIgnoringLineNames(autoRepeatValue);
                    if (trackSizes.size() >= expectedTrackCount)
                        break;
                }
            }
            break;
        }

        if (is<CSSGridIntegerRepeatValue>(currentValue)) {
            size_t repetitions = downcast<CSSGridIntegerRepeatValue>(currentValue.get()).repetitions();
            for (size_t i = 0; i < repetitions; ++i) {
                for (auto& integerRepeatValue : downcast<CSSValueList>(currentValue.get()))
                    handleValueIgnoringLineNames(integerRepeatValue);
            }
            continue;
        }

        handleValueIgnoringLineNames(currentValue);
    }
    
    return trackSizes;
}

static OrderedNamedGridLinesMap gridLineNames(const RenderStyle* renderStyle, GridTrackSizingDirection direction, unsigned expectedLineCount)
{
    if (!renderStyle)
        return { };
    
    OrderedNamedGridLinesMap combinedGridLineNames;
    auto appendLineNames = [&](unsigned index, const Vector<String>& newNames) {
        if (combinedGridLineNames.contains(index)) {
            auto names = combinedGridLineNames.take(index);
            names.appendVector(newNames);
            combinedGridLineNames.add(index, names);
        } else
            combinedGridLineNames.add(index, newNames);
    };
    
    auto orderedGridLineNames = direction == GridTrackSizingDirection::ForColumns ? renderStyle->orderedNamedGridColumnLines() : renderStyle->orderedNamedGridRowLines();
    for (auto& [i, names] : orderedGridLineNames)
        appendLineNames(i, names);
    
    auto autoRepeatOrderedGridLineNames = direction == GridTrackSizingDirection::ForColumns ? renderStyle->autoRepeatOrderedNamedGridColumnLines() : renderStyle->autoRepeatOrderedNamedGridRowLines();
    auto autoRepeatInsertionPoint = direction == GridTrackSizingDirection::ForColumns ? renderStyle->gridAutoRepeatColumnsInsertionPoint() : renderStyle->gridAutoRepeatRowsInsertionPoint();
    unsigned autoRepeatIndex = 0;
    while (autoRepeatOrderedGridLineNames.size() && autoRepeatIndex < expectedLineCount - autoRepeatInsertionPoint) {
        auto names = autoRepeatOrderedGridLineNames.get(autoRepeatIndex % autoRepeatOrderedGridLineNames.size());
        auto lineIndex = autoRepeatIndex + autoRepeatInsertionPoint;
        appendLineNames(lineIndex, names);
        ++autoRepeatIndex;
    }

    auto implicitGridLineNames = direction == GridTrackSizingDirection::ForColumns ? renderStyle->implicitNamedGridColumnLines() : renderStyle->implicitNamedGridRowLines();
    for (auto& [name, indexes] : implicitGridLineNames) {
        for (auto i : indexes)
            appendLineNames(i, {name});
    }
    
    return combinedGridLineNames;
}

static InspectorOverlay::Highlight::GridHighlightOverlay::Label buildLabel(String text, FloatPoint location, Color backgroundColor, InspectorOverlay::LabelArrowDirection arrowDirection, InspectorOverlay::LabelArrowEdgePosition arrowEdgePosition)
{
    InspectorOverlay::Highlight::GridHighlightOverlay::Label label;
    label.text = text;
    label.location = location;
    label.backgroundColor = backgroundColor;
    label.arrowDirection = arrowDirection;
    label.arrowEdgePosition = arrowEdgePosition;
    return label;
}

std::optional<InspectorOverlay::Highlight::GridHighlightOverlay> InspectorOverlay::buildGridOverlay(const InspectorOverlay::Grid& gridOverlay, bool offsetBoundsByScroll)
{
    // If the node WeakPtr has been cleared, then the node is gone and there's nothing to draw.
    if (!gridOverlay.gridNode) {
        m_activeGridOverlays.removeAllMatching([&] (const InspectorOverlay::Grid& gridOverlay) {
            return !gridOverlay.gridNode;
        });
        return { };
    }
    
    // Always re-check because the node's renderer may have changed since being added.
    // If renderer is no longer a grid, then remove the grid overlay for the node.
    Node* node = gridOverlay.gridNode.get();
    auto renderer = node->renderer();
    if (!is<RenderGrid>(renderer)) {
        removeGridOverlayForNode(*node);
        return { };
    }

    constexpr auto translucentLabelBackgroundColor = Color::white.colorWithAlphaByte(230);
    
    FrameView* pageView = m_page.mainFrame().view();
    if (!pageView)
        return { };
    FloatRect viewportBounds = { { 0, 0 }, pageView->sizeForVisibleContent() };

    auto scrollPosition = pageView->scrollPosition();
    if (offsetBoundsByScroll)
        viewportBounds.setLocation(scrollPosition);
    
    auto& renderGrid = *downcast<RenderGrid>(renderer);
    auto columnPositions = renderGrid.columnPositions();
    auto rowPositions = renderGrid.rowPositions();
    if (!columnPositions.size() || !rowPositions.size())
        return { };
    
    float gridStartX = columnPositions[0];
    float gridEndX = columnPositions[columnPositions.size() - 1];
    float gridStartY = rowPositions[0];
    float gridEndY = rowPositions[rowPositions.size() - 1];

    Frame* containingFrame = node->document().frame();
    if (!containingFrame)
        return { };
    FrameView* containingView = containingFrame->view();

    auto computedStyle = node->computedStyle();
    if (!computedStyle)
        return { };

    auto isHorizontalWritingMode = computedStyle->isHorizontalWritingMode();
    auto isDirectionFlipped = !computedStyle->isLeftToRightDirection();
    auto isWritingModeFlipped = computedStyle->isFlippedBlocksWritingMode();
    auto contentBox = renderGrid.absoluteBoundingBoxRectIgnoringTransforms();

    auto columnLineAt = [&](float x) -> FloatLine {
        FloatPoint startPoint;
        FloatPoint endPoint;
        if (isHorizontalWritingMode) {
            startPoint = { isDirectionFlipped ? contentBox.width() - x : x, isWritingModeFlipped ? contentBox.height() - gridStartY : gridStartY };
            endPoint = { isDirectionFlipped ? contentBox.width() - x : x, isWritingModeFlipped ? contentBox.height() - gridEndY : gridEndY };
        } else {
            startPoint = { isWritingModeFlipped ? contentBox.width() - gridStartY : gridStartY, isDirectionFlipped ? contentBox.height() - x : x };
            endPoint = { isWritingModeFlipped ? contentBox.width() - gridEndY : gridEndY, isDirectionFlipped ? contentBox.height() - x : x };
        }
        return {
            localPointToRootPoint(containingView, renderGrid.localToContainerPoint(startPoint, nullptr)),
            localPointToRootPoint(containingView, renderGrid.localToContainerPoint(endPoint, nullptr)),
        };
    };
    auto rowLineAt = [&](float y) -> FloatLine {
        FloatPoint startPoint;
        FloatPoint endPoint;
        if (isHorizontalWritingMode) {
            startPoint = { isDirectionFlipped ? contentBox.width() - gridStartX : gridStartX, isWritingModeFlipped ? contentBox.height() - y : y };
            endPoint = { isDirectionFlipped ? contentBox.width() - gridEndX : gridEndX, isWritingModeFlipped ? contentBox.height() - y : y };
        } else {
            startPoint = { isWritingModeFlipped ? contentBox.width() - y : y, isDirectionFlipped ? contentBox.height() - gridStartX : gridStartX };
            endPoint = { isWritingModeFlipped ? contentBox.width() - y : y, isDirectionFlipped ? contentBox.height() - gridEndX : gridEndX };
        }
        return {
            localPointToRootPoint(containingView, renderGrid.localToContainerPoint(startPoint, nullptr)),
            localPointToRootPoint(containingView, renderGrid.localToContainerPoint(endPoint, nullptr)),
        };
    };

    auto correctedArrowDirection = [&](LabelArrowDirection direction, GridTrackSizingDirection sizingDirection) -> LabelArrowDirection {
        if ((sizingDirection == GridTrackSizingDirection::ForColumns && isWritingModeFlipped) || (sizingDirection == GridTrackSizingDirection::ForRows && isDirectionFlipped)) {
            switch (direction) {
            case LabelArrowDirection::Down:
                direction = LabelArrowDirection::Up;
                break;
            case LabelArrowDirection::Up:
                direction = LabelArrowDirection::Down;
                break;
            case LabelArrowDirection::Left:
                direction = LabelArrowDirection::Right;
                break;
            case LabelArrowDirection::Right:
                direction = LabelArrowDirection::Left;
                break;
            case LabelArrowDirection::None:
                break;
            }
        }

        if (!isHorizontalWritingMode) {
            switch (direction) {
            case LabelArrowDirection::Down:
                direction = LabelArrowDirection::Right;
                break;
            case LabelArrowDirection::Up:
                direction = LabelArrowDirection::Left;
                break;
            case LabelArrowDirection::Left:
                direction = LabelArrowDirection::Up;
                break;
            case LabelArrowDirection::Right:
                direction = LabelArrowDirection::Down;
                break;
            case LabelArrowDirection::None:
                break;
            }
        }

        return direction;
    };

    auto correctedArrowEdgePosition = [&](LabelArrowEdgePosition edgePosition, GridTrackSizingDirection sizingDirection) -> LabelArrowEdgePosition {
        if ((sizingDirection == GridTrackSizingDirection::ForRows && isWritingModeFlipped) || (sizingDirection == GridTrackSizingDirection::ForColumns && isDirectionFlipped)) {
            if (edgePosition == LabelArrowEdgePosition::Leading)
                return LabelArrowEdgePosition::Trailing;
            if (edgePosition == LabelArrowEdgePosition::Trailing)
                return LabelArrowEdgePosition::Leading;
        }

        return edgePosition;
    };

    InspectorOverlay::Highlight::GridHighlightOverlay gridHighlightOverlay;
    gridHighlightOverlay.color = gridOverlay.config.gridColor;

    // Draw columns and rows.
    auto columnWidths = renderGrid.trackSizesForComputedStyle(GridTrackSizingDirection::ForColumns);
    auto columnLineNames = gridLineNames(node->renderStyle(), GridTrackSizingDirection::ForColumns, columnPositions.size());
    auto authoredTrackColumnSizes = authoredGridTrackSizes(node, GridTrackSizingDirection::ForColumns, columnWidths.size());
    FloatLine previousColumnEndLine;
    for (unsigned i = 0; i < columnPositions.size(); ++i) {
        auto columnStartLine = columnLineAt(columnPositions[i]);

        if (gridOverlay.config.showExtendedGridLines) {
            auto extendedLine = columnStartLine.extendedToBounds(viewportBounds);
            gridHighlightOverlay.gridLines.append(extendedLine);
        } else {
            gridHighlightOverlay.gridLines.append(columnStartLine);
        }
        
        FloatLine gapLabelLine = columnStartLine;
        if (i) {
            gridHighlightOverlay.gaps.append({ previousColumnEndLine.start(), columnStartLine.start(), columnStartLine.end(), previousColumnEndLine.end() });
            FloatLine lineBetweenColumnTops = { columnStartLine.start(), previousColumnEndLine.start() };
            FloatLine lineBetweenColumnBottoms = { columnStartLine.end(), previousColumnEndLine.end() };
            gapLabelLine = { lineBetweenColumnTops.pointAtRelativeDistance(0.5), lineBetweenColumnBottoms.pointAtRelativeDistance(0.5) };
        }

        if (i < columnWidths.size() && i < columnPositions.size()) {
            auto width = columnWidths[i];
            auto columnEndLine = columnLineAt(columnPositions[i] + width);

            if (gridOverlay.config.showExtendedGridLines) {
                auto extendedLine = columnEndLine.extendedToBounds(viewportBounds);
                gridHighlightOverlay.gridLines.append(extendedLine);
            } else {
                gridHighlightOverlay.gridLines.append(columnEndLine);
            }
            previousColumnEndLine = columnEndLine;
            
            if (gridOverlay.config.showTrackSizes) {
                auto authoredTrackSize = i < authoredTrackColumnSizes.size() ? authoredTrackColumnSizes[i] : "auto"_s;
                FloatLine trackTopLine = { columnStartLine.start(), columnEndLine.start() };
                gridHighlightOverlay.labels.append(buildLabel(authoredTrackSize, trackTopLine.pointAtRelativeDistance(0.5), translucentLabelBackgroundColor, correctedArrowDirection(LabelArrowDirection::Up, GridTrackSizingDirection::ForColumns), LabelArrowEdgePosition::Middle));
            }
        } else
            previousColumnEndLine = columnStartLine;

        StringBuilder lineLabel;
        if (gridOverlay.config.showLineNumbers) {
            lineLabel.append(i + 1);
            if (i <= authoredTrackColumnSizes.size())
                lineLabel.append(emSpace, -static_cast<int>(authoredTrackColumnSizes.size() - i + 1));
        }
        if (gridOverlay.config.showLineNames && columnLineNames.contains(i)) {
            for (auto lineName : columnLineNames.get(i)) {
                if (!lineLabel.isEmpty())
                    lineLabel.append(thinSpace, bullet, thinSpace);
                lineLabel.append(lineName);
            }
        }

        if (!lineLabel.isEmpty()) {
            auto text = lineLabel.toString();
            auto arrowDirection = correctedArrowDirection(LabelArrowDirection::Down, GridTrackSizingDirection::ForColumns);
            auto arrowEdgePosition = correctedArrowEdgePosition(LabelArrowEdgePosition::Middle, GridTrackSizingDirection::ForColumns);

            if (!i)
                arrowEdgePosition = correctedArrowEdgePosition(LabelArrowEdgePosition::Leading, GridTrackSizingDirection::ForColumns);
            else if (i == columnPositions.size() - 1)
                arrowEdgePosition = correctedArrowEdgePosition(LabelArrowEdgePosition::Trailing, GridTrackSizingDirection::ForColumns);

            auto expectedLabelSize = expectedSizeForLayoutLabel(text, arrowDirection);
            auto gapLabelPosition = gapLabelLine.start();

            // The area under the window's toolbar is drawable, but not meaningfully visible, so we must account for that space.
            auto topEdgeInset = pageView->topContentInset(ScrollView::TopContentInsetType::WebCoreOrPlatformContentInset);
            if (gapLabelLine.start().y() - expectedLabelSize.height() - topEdgeInset + scrollPosition.y() - viewportBounds.y() < 0) {
                arrowDirection = correctedArrowDirection(LabelArrowDirection::Up, GridTrackSizingDirection::ForColumns);

                // Special case for the first column to make sure the label will be out of the way of the first row's label.
                // The label heights will be the same, as they use the same font, so moving down by this label's size will
                // create enough space for this special circumstance.
                if (!i)
                    gapLabelPosition = gapLabelLine.pointAtAbsoluteDistance(expectedLabelSize.height());
            }

            gridHighlightOverlay.labels.append(buildLabel(text, gapLabelPosition, translucentLabelBackgroundColor, arrowDirection, arrowEdgePosition));
        }
    }

    auto rowHeights = renderGrid.trackSizesForComputedStyle(GridTrackSizingDirection::ForRows);
    auto rowLineNames = gridLineNames(node->renderStyle(), GridTrackSizingDirection::ForRows, rowPositions.size());
    auto authoredTrackRowSizes = authoredGridTrackSizes(node, GridTrackSizingDirection::ForRows, rowHeights.size());
    FloatLine previousRowEndLine;
    for (unsigned i = 0; i < rowPositions.size(); ++i) {
        auto rowStartLine = rowLineAt(rowPositions[i]);

        if (gridOverlay.config.showExtendedGridLines) {
            auto extendedLine = rowStartLine.extendedToBounds(viewportBounds);
            gridHighlightOverlay.gridLines.append(extendedLine);
        } else {
            gridHighlightOverlay.gridLines.append(rowStartLine);
        }

        FloatPoint gapLabelPosition = rowStartLine.start();
        if (i) {
            FloatLine lineBetweenRowStarts = { rowStartLine.start(), previousRowEndLine.start() };
            gridHighlightOverlay.gaps.append({ previousRowEndLine.start(), previousRowEndLine.end(), rowStartLine.end(), rowStartLine.start() });
            gapLabelPosition = lineBetweenRowStarts.pointAtRelativeDistance(0.5);
        }

        if (i < rowHeights.size() && i < rowPositions.size()) {
            auto height = rowHeights[i];
            auto rowEndLine = rowLineAt(rowPositions[i] + height);

            if (gridOverlay.config.showExtendedGridLines) {
                auto extendedLine = rowEndLine.extendedToBounds(viewportBounds);
                gridHighlightOverlay.gridLines.append(extendedLine);
            } else {
                gridHighlightOverlay.gridLines.append(rowEndLine);
            }
            previousRowEndLine = rowEndLine;

            if (gridOverlay.config.showTrackSizes) {
                auto authoredTrackSize = i < authoredTrackRowSizes.size() ? authoredTrackRowSizes[i] : "auto"_s;
                FloatLine trackLeftLine = { rowStartLine.start(), rowEndLine.start() };
                gridHighlightOverlay.labels.append(buildLabel(authoredTrackSize, trackLeftLine.pointAtRelativeDistance(0.5), translucentLabelBackgroundColor, correctedArrowDirection(LabelArrowDirection::Left, GridTrackSizingDirection::ForRows), LabelArrowEdgePosition::Middle));
            }
        } else
            previousRowEndLine = rowStartLine;

        StringBuilder lineLabel;
        if (gridOverlay.config.showLineNumbers) {
            lineLabel.append(i + 1);
            if (i <= authoredTrackRowSizes.size())
                lineLabel.append(emSpace, -static_cast<int>(authoredTrackRowSizes.size() - i + 1));
        }
        if (gridOverlay.config.showLineNames && rowLineNames.contains(i)) {
            for (auto lineName : rowLineNames.get(i)) {
                if (!lineLabel.isEmpty())
                    lineLabel.append(thinSpace, bullet, thinSpace);
                lineLabel.append(lineName);
            }
        }

        if (!lineLabel.isEmpty()) {
            auto text = lineLabel.toString();
            auto arrowDirection = correctedArrowDirection(LabelArrowDirection::Right, GridTrackSizingDirection::ForRows);
            auto arrowEdgePosition = correctedArrowEdgePosition(LabelArrowEdgePosition::Middle, GridTrackSizingDirection::ForRows);

            if (!i)
                arrowEdgePosition = correctedArrowEdgePosition(LabelArrowEdgePosition::Leading, GridTrackSizingDirection::ForRows);
            else if (i == rowPositions.size() - 1)
                arrowEdgePosition = correctedArrowEdgePosition(LabelArrowEdgePosition::Trailing, GridTrackSizingDirection::ForRows);

            auto expectedLabelSize = expectedSizeForLayoutLabel(text, arrowDirection);
            if (gapLabelPosition.x() - expectedLabelSize.width() + scrollPosition.x() - viewportBounds.x() < 0)
                arrowDirection = correctedArrowDirection(LabelArrowDirection::Left, GridTrackSizingDirection::ForRows);

            gridHighlightOverlay.labels.append(buildLabel(text, gapLabelPosition, translucentLabelBackgroundColor, arrowDirection, arrowEdgePosition));
        }
    }

    if (gridOverlay.config.showAreaNames) {
        for (auto& gridArea : node->renderStyle()->namedGridArea()) {
            auto& name = gridArea.key;
            auto& area = gridArea.value;

            // Named grid areas will always be rectangular per the CSS Grid specification.
            auto columnStartLine = columnLineAt(columnPositions[area.columns.startLine()]);
            auto columnEndLine = columnLineAt(columnPositions[area.columns.endLine() - 1] + columnWidths[area.columns.endLine() - 1]);
            auto rowStartLine = rowLineAt(rowPositions[area.rows.startLine()]);
            auto rowEndLine = rowLineAt(rowPositions[area.rows.endLine() - 1] + rowHeights[area.rows.endLine() - 1]);

            std::optional<FloatPoint> topLeft = columnStartLine.intersectionWith(rowStartLine);
            std::optional<FloatPoint> topRight = columnEndLine.intersectionWith(rowStartLine);
            std::optional<FloatPoint> bottomRight = columnEndLine.intersectionWith(rowEndLine);
            std::optional<FloatPoint> bottomLeft = columnStartLine.intersectionWith(rowEndLine);

            // If any two lines are coincident with each other, they will not have an intersection, which can occur with extreme `transform: perspective(...)` values.
            if (!topLeft || !topRight || !bottomRight || !bottomLeft)
                continue;

            InspectorOverlay::Highlight::GridHighlightOverlay::Area highlightOverlayArea;
            highlightOverlayArea.name = name;
            highlightOverlayArea.quad = { *topLeft, *topRight, *bottomRight, *bottomLeft };
            gridHighlightOverlay.areas.append(highlightOverlayArea);
        }
    }

    return { gridHighlightOverlay };
}

} // namespace WebCore
