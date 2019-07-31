/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "DOMCSSNamespace.h"
#include "DOMTokenList.h"
#include "Element.h"
#include "FloatPoint.h"
#include "FloatRoundedRect.h"
#include "FloatSize.h"
#include "FontCascade.h"
#include "FontCascadeDescription.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "InspectorClient.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Node.h"
#include "NodeList.h"
#include "Page.h"
#include "PseudoElement.h"
#include "RenderBox.h"
#include "RenderBoxModelObject.h"
#include "RenderInline.h"
#include "RenderObject.h"
#include "Settings.h"
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

static constexpr UChar ellipsis = 0x2026;
static constexpr UChar multiplicationSign = 0x00D7;

static void truncateWithEllipsis(String& string, size_t length)
{
    if (string.length() > length) {
        string.truncate(length);
        string.append(ellipsis);
    }
}

static FloatPoint localPointToRootPoint(const FrameView* view, const FloatPoint& point)
{
    return view->contentsToRootView(roundedIntPoint(point));
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

static Path quadToPath(const FloatQuad& quad, Highlight::Bounds& bounds)
{
    Path path;
    path.moveTo(quad.p1());
    path.addLineTo(quad.p2());
    path.addLineTo(quad.p3());
    path.addLineTo(quad.p4());
    path.closeSubpath();

    bounds.unite(path.boundingRect());

    return path;
}

static void drawOutlinedQuadWithClip(GraphicsContext& context, const FloatQuad& quad, const FloatQuad& clipQuad, const Color& fillColor, Highlight::Bounds& bounds)
{
    GraphicsContextStateSaver stateSaver(context);

    context.setFillColor(fillColor);
    context.setStrokeThickness(0);
    context.fillPath(quadToPath(quad, bounds));

    context.setCompositeOperation(CompositeDestinationOut);
    context.setFillColor(Color::createUnchecked(255, 0, 0));
    context.fillPath(quadToPath(clipQuad, bounds));
}

static void drawOutlinedQuad(GraphicsContext& context, const FloatQuad& quad, const Color& fillColor, const Color& outlineColor, Highlight::Bounds& bounds)
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

static void drawFragmentHighlight(GraphicsContext& context, Node& node, const HighlightConfig& highlightConfig, Highlight::Bounds& bounds)
{
    Highlight highlight;
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

    if (!marginQuad.isEmpty() && marginQuad != borderQuad && highlight.marginColor.isVisible())
        drawOutlinedQuadWithClip(context, marginQuad, borderQuad, highlight.marginColor, bounds);

    if (!borderQuad.isEmpty() && borderQuad != paddingQuad && highlight.borderColor.isVisible())
        drawOutlinedQuadWithClip(context, borderQuad, paddingQuad, highlight.borderColor, bounds);

    if (!paddingQuad.isEmpty() && paddingQuad != contentQuad && highlight.paddingColor.isVisible())
        drawOutlinedQuadWithClip(context, paddingQuad, contentQuad, highlight.paddingColor, bounds);

    if (!contentQuad.isEmpty() && (highlight.contentColor.isVisible() || highlight.contentOutlineColor.isVisible()))
        drawOutlinedQuad(context, contentQuad, highlight.contentColor, highlight.contentOutlineColor, bounds);
}

static void drawShapeHighlight(GraphicsContext& context, Node& node, Highlight::Bounds& bounds)
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

    const Color shapeHighlightColor(96, 82, 127, 204);

    Shape::DisplayPaths paths;
    shapeOutsideInfo->computedShape().buildDisplayPaths(paths);

    if (paths.shape.isEmpty()) {
        LayoutRect shapeBounds = shapeOutsideInfo->computedShapePhysicalBoundingBox();
        FloatQuad shapeQuad = renderer->localToAbsoluteQuad(FloatRect(shapeBounds));
        contentsQuadToCoordinateSystem(mainView, containingView, shapeQuad, InspectorOverlay::CoordinateSystem::Document);
        drawOutlinedQuad(context, shapeQuad, shapeHighlightColor, Color::transparent, bounds);
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
            case PathElementMoveToPoint:
                newPath.moveTo(localToRoot(0));
                break;

            case PathElementAddLineToPoint:
                newPath.addLineTo(localToRoot(0));
                break;

            case PathElementAddCurveToPoint:
                newPath.addBezierCurveTo(localToRoot(0), localToRoot(1), localToRoot(2));
                break;

            case PathElementAddQuadCurveToPoint:
                newPath.addQuadCurveTo(localToRoot(0), localToRoot(1));
                break;

            case PathElementCloseSubpath:
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

        const Color shapeMarginHighlightColor(96, 82, 127, 153);
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

        const Color indicatingColor(111, 168, 220, 168);
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

    if (!m_paintRects.isEmpty())
        drawPaintRects(context, m_paintRects);

    if (m_showRulers || m_showRulersDuringElementSelection)
        drawRulers(context, rulerExclusion);
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
    if (m_indicating == indicating)
        return;

    m_indicating = indicating;

    update();
}

bool InspectorOverlay::shouldShowOverlay() const
{
    // Don't show the overlay when m_showRulersDuringElementSelection is true, as it's only supposed
    // to have an effect when element selection is active (e.g. a node is hovered).
    return m_highlightNode || m_highlightNodeList || m_highlightQuad || m_indicating || m_showPaintRects || m_showRulers;
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

    return rulerExclusion;
}

InspectorOverlay::RulerExclusion InspectorOverlay::drawQuadHighlight(GraphicsContext& context, const FloatQuad& quad)
{
    RulerExclusion rulerExclusion;

    Highlight highlight;
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

    const Color paintRectsColor(1.0f, 0.0f, 0.0f, 0.5f);
    context.setFillColor(paintRectsColor);

    for (const TimeRectPair& pair : paintRects)
        context.fillRect(pair.second);
}

void InspectorOverlay::drawBounds(GraphicsContext& context, const Highlight::Bounds& bounds)
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

    const Color boundsColor(1.0f, 0.0f, 0.0f, 0.6f);
    context.setStrokeColor(boundsColor);

    context.strokePath(path);
}

void InspectorOverlay::drawRulers(GraphicsContext& context, const InspectorOverlay::RulerExclusion& rulerExclusion)
{
    const Color rulerBackgroundColor(1.0f, 1.0f, 1.0f, 0.6f);
    const Color lightRulerColor(0.0f, 0.0f, 0.0f, 0.2f);
    const Color darkRulerColor(0.0f, 0.0f, 0.0f, 0.5f);

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
                context.drawText(font, TextRun(String::numberToStringFixedPrecision(x)), { 2, drawTopEdge ? rulerLabelSize : rulerLabelSize - rulerSize + font.fontMetrics().height() - 1.0f });
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
                context.drawText(font, TextRun(String::numberToStringFixedPrecision(y)), { 2, drawLeftEdge ? rulerLabelSize : rulerLabelSize - rulerSize });
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
        auto viewportWidthText = String::numberToStringFixedPrecision(viewportRect.width() / pageZoomFactor);
        auto viewportHeightText = String::numberToStringFixedPrecision(viewportRect.height() / pageZoomFactor);
        TextRun viewportTextRun(makeString(viewportWidthText, "px", ' ', multiplicationSign, ' ', viewportHeightText, "px"));

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

Path InspectorOverlay::drawElementTitle(GraphicsContext& context, Node& node, const Highlight::Bounds& bounds)
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
        String firstLine = makeString(elementTagName, elementIDValue, elementClassValue, elementPseudoType, ' ', elementWidth, "px", ' ', multiplicationSign, ' ', elementHeight, "px");
        String secondLine = makeString("Role ", elementRole);

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

    const Color elementTitleBackgroundColor(255, 255, 194);
    context.setFillColor(elementTitleBackgroundColor);

    context.fillPath(path);

    context.setStrokeThickness(elementDataBorderSize);

    const Color elementTitleBorderColor(128, 128, 128);
    context.setStrokeColor(elementTitleBorderColor);

    context.strokePath(path);

    float textPositionX = boxX + elementDataSpacing;
    float textPositionY = boxY - (elementDataSpacing / 2.0f) + fontHeight;
    const auto drawText = [&] (const String& text, const Color& color) {
        if (text.isEmpty())
            return;

        context.setFillColor(color);
        textPositionX += context.drawText(font, TextRun(text), { textPositionX, textPositionY });
    };

    drawText(elementTagName, Color(136, 18, 128)); // Keep this in sync with XMLViewer.css (.tag)
    drawText(elementIDValue, Color(26, 26, 166)); // Keep this in sync with XMLViewer.css (.attribute-value)
    drawText(elementClassValue, Color(153, 69, 0)); // Keep this in sync with XMLViewer.css (.attribute-name)
    drawText(elementPseudoType, Color(136, 18, 128)); // Keep this in sync with XMLViewer.css (.tag)
    drawText(" "_s, Color::black);
    drawText(elementWidth, Color::black);
    drawText("px"_s, Color::darkGray);
    drawText(" "_s, Color::darkGray);
    drawText(makeString(multiplicationSign), Color::darkGray);
    drawText(" "_s, Color::darkGray);
    drawText(elementHeight, Color::black);
    drawText("px"_s, Color::darkGray);

    if (hasSecondLine) {
        textPositionX = boxX + elementDataSpacing;
        textPositionY += fontHeight;

        drawText("Role"_s, Color(170, 13, 145));
        drawText(" "_s, Color::black);
        drawText(elementRole, Color::black);
    }

    return path;
}

} // namespace WebCore
