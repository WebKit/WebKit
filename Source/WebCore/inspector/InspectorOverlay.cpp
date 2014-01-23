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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#if ENABLE(INSPECTOR)

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
#include "PolygonShape.h"
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
#include <inspector/InspectorValues.h>
#include <wtf/text/StringBuilder.h>

using namespace Inspector;

namespace WebCore {

namespace {

Path quadToPath(const FloatQuad& quad)
{
    Path quadPath;
    quadPath.moveTo(quad.p1());
    quadPath.addLineTo(quad.p2());
    quadPath.addLineTo(quad.p3());
    quadPath.addLineTo(quad.p4());
    quadPath.closeSubpath();
    return quadPath;
}

void drawOutlinedQuad(GraphicsContext* context, const FloatQuad& quad, const Color& fillColor, const Color& outlineColor)
{
    static const int outlineThickness = 2;

    Path quadPath = quadToPath(quad);

    // Clip out the quad, then draw with a 2px stroke to get a pixel
    // of outline (because inflating a quad is hard)
    {
        context->save();
        context->clipOut(quadPath);

        context->setStrokeThickness(outlineThickness);
        context->setStrokeColor(outlineColor, ColorSpaceDeviceRGB);
        context->strokePath(quadPath);

        context->restore();
    }

    // Now do the fill
    context->setFillColor(fillColor, ColorSpaceDeviceRGB);
    context->fillPath(quadPath);
}

static void contentsQuadToPage(const FrameView* mainView, const FrameView* view, FloatQuad& quad)
{
    quad.setP1(view->contentsToRootView(roundedIntPoint(quad.p1())));
    quad.setP2(view->contentsToRootView(roundedIntPoint(quad.p2())));
    quad.setP3(view->contentsToRootView(roundedIntPoint(quad.p3())));
    quad.setP4(view->contentsToRootView(roundedIntPoint(quad.p4())));
    quad += mainView->scrollOffset();
}

static void buildRendererHighlight(RenderObject* renderer, RenderRegion* region, const HighlightConfig& highlightConfig, Highlight* highlight)
{
    Frame* containingFrame = renderer->document().frame();
    if (!containingFrame)
        return;

    highlight->setDataFromConfig(highlightConfig);
    FrameView* containingView = containingFrame->view();
    FrameView* mainView = containingFrame->page()->mainFrame().view();

    // RenderSVGRoot should be highlighted through the isBox() code path, all other SVG elements should just dump their absoluteQuads().
#if ENABLE(SVG)
    bool isSVGRenderer = renderer->node() && renderer->node()->isSVGElement() && !renderer->isSVGRoot();
#else
    bool isSVGRenderer = false;
#endif

    if (isSVGRenderer) {
        highlight->type = HighlightTypeRects;
        renderer->absoluteQuads(highlight->quads);
        for (size_t i = 0; i < highlight->quads.size(); ++i)
            contentsQuadToPage(mainView, containingView, highlight->quads[i]);
    } else if (renderer->isBox() || renderer->isRenderInline()) {
        LayoutRect contentBox;
        LayoutRect paddingBox;
        LayoutRect borderBox;
        LayoutRect marginBox;

        if (renderer->isBox()) {
            RenderBox* renderBox = toRenderBox(renderer);

            LayoutBoxExtent margins(renderBox->marginTop(), renderBox->marginRight(), renderBox->marginBottom(), renderBox->marginLeft());

            if (!renderBox->isOutOfFlowPositioned() && region) {
                RenderBox::LogicalExtentComputedValues computedValues;
                renderBox->computeLogicalWidthInRegion(computedValues, region);
                margins.mutableLogicalLeft(renderBox->style().writingMode()) = computedValues.m_margins.m_start;
                margins.mutableLogicalRight(renderBox->style().writingMode()) = computedValues.m_margins.m_end;
            }

            paddingBox = renderBox->clientBoxRectInRegion(region);
            contentBox = LayoutRect(paddingBox.x() + renderBox->paddingLeft(), paddingBox.y() + renderBox->paddingTop(),
                paddingBox.width() - renderBox->paddingLeft() - renderBox->paddingRight(), paddingBox.height() - renderBox->paddingTop() - renderBox->paddingBottom());
            borderBox = LayoutRect(paddingBox.x() - renderBox->borderLeft(), paddingBox.y() - renderBox->borderTop(),
                paddingBox.width() + renderBox->borderLeft() + renderBox->borderRight(), paddingBox.height() + renderBox->borderTop() + renderBox->borderBottom());
            marginBox = LayoutRect(borderBox.x() - margins.left(), borderBox.y() - margins.top(),
                borderBox.width() + margins.left() + margins.right(), borderBox.height() + margins.top() + margins.bottom());
        } else {
            RenderInline* renderInline = toRenderInline(renderer);

            // RenderInline's bounding box includes paddings and borders, excludes margins.
            borderBox = renderInline->linesBoundingBox();
            paddingBox = LayoutRect(borderBox.x() + renderInline->borderLeft(), borderBox.y() + renderInline->borderTop(),
                borderBox.width() - renderInline->borderLeft() - renderInline->borderRight(), borderBox.height() - renderInline->borderTop() - renderInline->borderBottom());
            contentBox = LayoutRect(paddingBox.x() + renderInline->paddingLeft(), paddingBox.y() + renderInline->paddingTop(),
                paddingBox.width() - renderInline->paddingLeft() - renderInline->paddingRight(), paddingBox.height() - renderInline->paddingTop() - renderInline->paddingBottom());
            // Ignore marginTop and marginBottom for inlines.
            marginBox = LayoutRect(borderBox.x() - renderInline->marginLeft(), borderBox.y(),
                borderBox.width() + renderInline->marginWidth(), borderBox.height());
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

        contentsQuadToPage(mainView, containingView, absContentQuad);
        contentsQuadToPage(mainView, containingView, absPaddingQuad);
        contentsQuadToPage(mainView, containingView, absBorderQuad);
        contentsQuadToPage(mainView, containingView, absMarginQuad);

        highlight->type = HighlightTypeNode;
        highlight->quads.append(absMarginQuad);
        highlight->quads.append(absBorderQuad);
        highlight->quads.append(absPaddingQuad);
        highlight->quads.append(absContentQuad);
    }
}

static void buildNodeHighlight(Node* node, RenderRegion* region, const HighlightConfig& highlightConfig, Highlight* highlight)
{
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return;
    buildRendererHighlight(renderer, region, highlightConfig, highlight);
}

static void buildQuadHighlight(const FloatQuad& quad, const HighlightConfig& highlightConfig, Highlight *highlight)
{
    highlight->setDataFromConfig(highlightConfig);
    highlight->type = HighlightTypeRects;
    highlight->quads.append(quad);
}

} // anonymous namespace

InspectorOverlay::InspectorOverlay(Page& page, InspectorClient* client)
    : m_page(page)
    , m_client(client)
{
}

InspectorOverlay::~InspectorOverlay()
{
}

void InspectorOverlay::paint(GraphicsContext& context)
{
    if (m_pausedInDebuggerMessage.isNull() && !m_highlightNode && !m_highlightQuad && m_size.isEmpty())
        return;
    GraphicsContextStateSaver stateSaver(context);
    FrameView* view = overlayPage()->mainFrame().view();
    view->updateLayoutAndStyleIfNeededRecursive();
    view->paint(&context, IntRect(0, 0, view->width(), view->height()));
}

void InspectorOverlay::drawOutline(GraphicsContext* context, const LayoutRect& rect, const Color& color)
{
    FloatRect outlineRect = rect;
    drawOutlinedQuad(context, outlineRect, Color(), color);
}

void InspectorOverlay::getHighlight(Highlight* highlight) const
{
    if (!m_highlightNode && !m_highlightQuad)
        return;

    highlight->type = HighlightTypeRects;
    if (m_highlightNode)
        buildNodeHighlight(m_highlightNode.get(), nullptr, m_nodeHighlightConfig, highlight);
    else
        buildQuadHighlight(*m_highlightQuad, m_quadHighlightConfig, highlight);
}

void InspectorOverlay::setPausedInDebuggerMessage(const String* message)
{
    m_pausedInDebuggerMessage = message ? *message : String();
    update();
}

void InspectorOverlay::hideHighlight()
{
    m_highlightNode.clear();
    m_highlightQuad.clear();
    update();
}

void InspectorOverlay::highlightNode(Node* node, const HighlightConfig& highlightConfig)
{
    m_nodeHighlightConfig = highlightConfig;
    m_highlightNode = node;
    update();
}

void InspectorOverlay::highlightQuad(PassOwnPtr<FloatQuad> quad, const HighlightConfig& highlightConfig)
{
    if (m_quadHighlightConfig.usePageCoordinates)
        *quad -= m_page.mainFrame().view()->scrollOffset();

    m_quadHighlightConfig = highlightConfig;
    m_highlightQuad = quad;
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

void InspectorOverlay::update()
{
    if (!m_highlightNode && !m_highlightQuad && m_pausedInDebuggerMessage.isNull() && m_size.isEmpty()) {
        m_client->hideHighlight();
        return;
    }

    FrameView* view = m_page.mainFrame().view();
    if (!view)
        return;

    FrameView* overlayView = overlayPage()->mainFrame().view();
    IntSize viewportSize = view->visibleContentRect().size();
    IntSize frameViewFullSize = view->visibleContentRect(ScrollableArea::IncludeScrollbars).size();
    IntSize size = m_size.isEmpty() ? frameViewFullSize : m_size;
    overlayPage()->setPageScaleFactor(m_page.pageScaleFactor(), IntPoint());
    size.scale(m_page.pageScaleFactor());
    overlayView->resize(size);

    // Clear canvas and paint things.
    reset(viewportSize, m_size.isEmpty() ? IntSize() : frameViewFullSize);

    // Include scrollbars to avoid masking them by the gutter.
    drawGutter();
    drawNodeHighlight();
    drawQuadHighlight();
    drawPausedInDebuggerMessage();

    // Position DOM elements.
    overlayPage()->mainFrame().document()->recalcStyle(Style::Force);
    if (overlayView->needsLayout())
        overlayView->layout();

    // Kick paint.
    m_client->highlight();
}

static PassRefPtr<InspectorObject> buildObjectForPoint(const FloatPoint& point)
{
    RefPtr<InspectorObject> object = InspectorObject::create();
    object->setNumber("x", point.x());
    object->setNumber("y", point.y());
    return object.release();
}

static PassRefPtr<InspectorArray> buildArrayForQuad(const FloatQuad& quad)
{
    RefPtr<InspectorArray> array = InspectorArray::create();
    array->pushObject(buildObjectForPoint(quad.p1()));
    array->pushObject(buildObjectForPoint(quad.p2()));
    array->pushObject(buildObjectForPoint(quad.p3()));
    array->pushObject(buildObjectForPoint(quad.p4()));
    return array.release();
}

static PassRefPtr<InspectorObject> buildObjectForHighlight(const Highlight& highlight)
{
    RefPtr<InspectorObject> object = InspectorObject::create();
    RefPtr<InspectorArray> array = InspectorArray::create();
    for (size_t i = 0; i < highlight.quads.size(); ++i)
        array->pushArray(buildArrayForQuad(highlight.quads[i]));
    object->setArray("quads", array.release());
    object->setBoolean("showRulers", highlight.showRulers);
    object->setString("contentColor", highlight.contentColor.serialized());
    object->setString("contentOutlineColor", highlight.contentOutlineColor.serialized());
    object->setString("paddingColor", highlight.paddingColor.serialized());
    object->setString("borderColor", highlight.borderColor.serialized());
    object->setString("marginColor", highlight.marginColor.serialized());
    return object.release();
}

static PassRefPtr<InspectorObject> buildObjectForRegionHighlight(FrameView* mainView, RenderRegion* region)
{
    FrameView* containingView = region->frame().view();
    if (!containingView)
        return nullptr;

    RenderBlockFlow* regionContainer = toRenderBlockFlow(region->parent());
    LayoutRect borderBox = regionContainer->borderBoxRect();
    borderBox.setWidth(borderBox.width() + regionContainer->verticalScrollbarWidth());
    borderBox.setHeight(borderBox.height() + regionContainer->horizontalScrollbarHeight());

    // Create incoming and outgoing boxes that we use to chain the regions toghether.
    const LayoutSize linkBoxSize(10, 10);
    const LayoutSize linkBoxMidpoint(linkBoxSize.width() / 2, linkBoxSize.height() / 2);
    
    LayoutRect incomingRectBox = LayoutRect(borderBox.location() - linkBoxMidpoint, linkBoxSize);
    LayoutRect outgoingRectBox = LayoutRect(borderBox.location() - linkBoxMidpoint + borderBox.size(), linkBoxSize);

    // Move the link boxes slightly inside the region border box.
    LayoutUnit maxUsableHeight = std::max(LayoutUnit(), borderBox.height() - linkBoxMidpoint.height());
    LayoutUnit linkBoxVerticalOffset = std::min(LayoutUnit(15), maxUsableHeight);
    incomingRectBox.move(0, linkBoxVerticalOffset);
    outgoingRectBox.move(0, -linkBoxVerticalOffset);

    FloatQuad borderRectQuad = regionContainer->localToAbsoluteQuad(FloatRect(borderBox));
    FloatQuad incomingRectQuad = regionContainer->localToAbsoluteQuad(FloatRect(incomingRectBox));
    FloatQuad outgoingRectQuad = regionContainer->localToAbsoluteQuad(FloatRect(outgoingRectBox));

    contentsQuadToPage(mainView, containingView, borderRectQuad);
    contentsQuadToPage(mainView, containingView, incomingRectQuad);
    contentsQuadToPage(mainView, containingView, outgoingRectQuad);

    RefPtr<InspectorObject> regionObject = InspectorObject::create();

    regionObject->setArray("borderQuad", buildArrayForQuad(borderRectQuad));
    regionObject->setArray("incomingQuad", buildArrayForQuad(incomingRectQuad));
    regionObject->setArray("outgoingQuad", buildArrayForQuad(outgoingRectQuad));

    return regionObject.release();
}

static PassRefPtr<InspectorArray> buildObjectForCSSRegionsHighlight(RenderRegion* region, RenderFlowThread* flowThread)
{
    FrameView* mainFrameView = region->document().page()->mainFrame().view();

    RefPtr<InspectorArray> array = InspectorArray::create();

    const RenderRegionList& regionList = flowThread->renderRegionList();
    for (auto& iterRegion : regionList) {
        if (!iterRegion->isValid())
            continue;
        RefPtr<InspectorObject> regionHighlightObject = buildObjectForRegionHighlight(mainFrameView, iterRegion);
        if (!regionHighlightObject)
            continue;
        if (region == iterRegion) {
            // Let the script know that this is the currently highlighted node.
            regionHighlightObject->setBoolean("isHighlighted", true);
        }
        array->pushObject(regionHighlightObject.release());
    }

    return array.release();
}

static PassRefPtr<InspectorObject> buildObjectForSize(const IntSize& size)
{
    RefPtr<InspectorObject> result = InspectorObject::create();
    result->setNumber("width", size.width());
    result->setNumber("height", size.height());
    return result.release();
}

static PassRefPtr<InspectorObject> buildObjectForCSSRegionContentClip(RenderRegion* region)
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

    RefPtr<InspectorObject> regionObject = InspectorObject::create();
    regionObject->setArray("quad", buildArrayForQuad(clipQuad));
    return regionObject.release();
}

void InspectorOverlay::drawGutter()
{
    evaluateInOverlay("drawGutter", "");
}

static PassRefPtr<InspectorArray> buildObjectForRendererFragments(RenderObject* renderer, const HighlightConfig& config)
{
    RefPtr<InspectorArray> fragmentsArray = InspectorArray::create();

    RenderFlowThread* containingFlowThread = renderer->flowThreadContainingBlock();
    if (!containingFlowThread) {
        Highlight highlight;
        buildRendererHighlight(renderer, nullptr, config, &highlight);
        fragmentsArray->pushObject(buildObjectForHighlight(highlight));
    } else {
        RenderBox* enclosingBox = renderer->enclosingBox();
        RenderRegion* startRegion = nullptr;
        RenderRegion* endRegion = nullptr;
        containingFlowThread->getRegionRangeForBox(enclosingBox, startRegion, endRegion);
        if (!startRegion) {
            // The flow has no visible regions. The renderer is not visible on screen.
            return nullptr;
        }
        const RenderRegionList& regionList = containingFlowThread->renderRegionList();
        for (RenderRegionList::const_iterator iter = regionList.find(startRegion); iter != regionList.end(); ++iter) {
            RenderRegion* region = *iter;
            if (region->isValid()) {
                // Compute the highlight of the fragment inside the current region.
                Highlight highlight;
                buildRendererHighlight(renderer, region, config, &highlight);
                RefPtr<InspectorObject> fragmentObject = buildObjectForHighlight(highlight);

                // Compute the clipping area of the region.
                fragmentObject->setObject("region", buildObjectForCSSRegionContentClip(region));
                fragmentsArray->pushObject(fragmentObject.release());
            }
            if (region == endRegion)
                break;
        }
    }

    return fragmentsArray.release();
}

#if ENABLE(CSS_SHAPES)
static FloatPoint localPointToRoot(RenderObject* renderer, const FrameView* mainView, const FrameView* view, const FloatPoint& point)
{
    FloatPoint result = renderer->localToAbsolute(point);
    result = view->contentsToRootView(roundedIntPoint(result));
    result += mainView->scrollOffset();
    return result;
}

struct PathApplyInfo {
    FrameView* rootView;
    FrameView* view;
    InspectorArray* array;
    RenderObject* renderer;
    const ShapeOutsideInfo* shapeOutsideInfo;
};

static void appendPathCommandAndPoints(PathApplyInfo* info, const String& command, const FloatPoint points[], unsigned length)
{
    FloatPoint point;
    info->array->pushString(command);
    for (unsigned i = 0; i < length; i++) {
        point = info->shapeOutsideInfo->shapeToRendererPoint(points[i]);
        point = localPointToRoot(info->renderer, info->rootView, info->view, point);
        info->array->pushNumber(point.x());
        info->array->pushNumber(point.y());
    }
}

static void appendPathSegment(void* info, const PathElement* pathElement)
{
    PathApplyInfo* pathApplyInfo = static_cast<PathApplyInfo*>(info);
    FloatPoint point;
    switch (pathElement->type) {
    // The points member will contain 1 value.
    case PathElementMoveToPoint:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("M"), pathElement->points, 1);
        break;
    // The points member will contain 1 value.
    case PathElementAddLineToPoint:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("L"), pathElement->points, 1);
        break;
    // The points member will contain 3 values.
    case PathElementAddCurveToPoint:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("C"), pathElement->points, 3);
        break;
    // The points member will contain 2 values.
    case PathElementAddQuadCurveToPoint:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("Q"), pathElement->points, 2);
        break;
    // The points member will contain no values.
    case PathElementCloseSubpath:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("Z"), nullptr, 0);
        break;
    }
}

static PassRefPtr<InspectorObject> buildObjectForShapeOutside(Frame* containingFrame, RenderBox* renderer)
{
    const ShapeOutsideInfo* shapeOutsideInfo = renderer->shapeOutsideInfo();
    if (!shapeOutsideInfo)
        return nullptr;

    RefPtr<InspectorObject> shapeObject = InspectorObject::create();
    LayoutRect shapeBounds = shapeOutsideInfo->computedShapePhysicalBoundingBox();
    FloatQuad shapeQuad = renderer->localToAbsoluteQuad(FloatRect(shapeBounds));
    contentsQuadToPage(containingFrame->page()->mainFrame().view(), containingFrame->view(), shapeQuad);
    shapeObject->setArray(ASCIILiteral("bounds"), buildArrayForQuad(shapeQuad));

    Shape::DisplayPaths paths;
    shapeOutsideInfo->computedShape().buildDisplayPaths(paths);

    if (paths.shape.length()) {
        RefPtr<InspectorArray> shapePath = InspectorArray::create();
        PathApplyInfo info;
        info.rootView = containingFrame->page()->mainFrame().view();
        info.view = containingFrame->view();
        info.array = shapePath.get();
        info.renderer = renderer;
        info.shapeOutsideInfo = shapeOutsideInfo;

        paths.shape.apply(&info, &appendPathSegment);

        shapeObject->setArray(ASCIILiteral("shape"), shapePath.release());

        if (paths.marginShape.length()) {
            shapePath = InspectorArray::create();
            info.array = shapePath.get();

            paths.marginShape.apply(&info, &appendPathSegment);

            shapeObject->setArray(ASCIILiteral("marginShape"), shapePath.release());
        }
    }

    return shapeObject.release();
}
#endif

static PassRefPtr<InspectorObject> buildObjectForElementInfo(Node* node)
{
    if (!node->isElementNode() || !node->document().frame())
        return nullptr;

    RefPtr<InspectorObject> elementInfo = InspectorObject::create();

    Element* element = toElement(node);
    bool isXHTML = element->document().isXHTMLDocument();
    elementInfo->setString("tagName", isXHTML ? element->nodeName() : element->nodeName().lower());
    elementInfo->setString("idValue", element->getIdAttribute());
    HashSet<AtomicString> usedClassNames;
    if (element->hasClass() && element->isStyledElement()) {
        StringBuilder classNames;
        const SpaceSplitString& classNamesString = toStyledElement(element)->classNames();
        size_t classNameCount = classNamesString.size();
        for (size_t i = 0; i < classNameCount; ++i) {
            const AtomicString& className = classNamesString[i];
            if (usedClassNames.contains(className))
                continue;
            usedClassNames.add(className);
            classNames.append('.');
            classNames.append(className);
        }
        elementInfo->setString("className", classNames.toString());
    }

    RenderElement* renderer = element->renderer();
    Frame* containingFrame = node->document().frame();
    FrameView* containingView = containingFrame->view();
    IntRect boundingBox = pixelSnappedIntRect(containingView->contentsToRootView(renderer->absoluteBoundingBoxRect()));
    RenderBoxModelObject* modelObject = renderer->isBoxModelObject() ? toRenderBoxModelObject(renderer) : nullptr;
    elementInfo->setString("nodeWidth", String::number(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetWidth(), *modelObject) : boundingBox.width()));
    elementInfo->setString("nodeHeight", String::number(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetHeight(), *modelObject) : boundingBox.height()));
    
    if (renderer->isRenderNamedFlowFragmentContainer()) {
        RenderNamedFlowFragment* region = toRenderBlockFlow(renderer)->renderNamedFlowFragment();
        RenderFlowThread* flowThread = region->flowThread();
        if (flowThread && flowThread->isRenderNamedFlowThread()) {
            RefPtr<InspectorObject> regionFlowInfo = InspectorObject::create();
            regionFlowInfo->setString("name", toRenderNamedFlowThread(flowThread)->flowThreadName());
            regionFlowInfo->setArray("regions", buildObjectForCSSRegionsHighlight(region, flowThread));
            elementInfo->setObject("regionFlowInfo", regionFlowInfo.release());
        }
    }

    RenderFlowThread* containingFlowThread = renderer->flowThreadContainingBlock();
    if (containingFlowThread && containingFlowThread->isRenderNamedFlowThread()) {
        RefPtr<InspectorObject> contentFlowInfo = InspectorObject::create();
        contentFlowInfo->setString("name", toRenderNamedFlowThread(containingFlowThread)->flowThreadName());
        elementInfo->setObject("contentFlowInfo", contentFlowInfo.release());
    }

#if ENABLE(CSS_SHAPES)
    if (renderer->isBox()) {
        RenderBox* renderBox = toRenderBox(renderer);
        if (RefPtr<InspectorObject> shapeObject = buildObjectForShapeOutside(containingFrame, renderBox))
            elementInfo->setObject("shapeOutsideInfo", shapeObject.release());
    }
#endif

    return elementInfo.release();
}

PassRefPtr<InspectorObject> InspectorOverlay::buildObjectForHighlightedNode() const
{
    if (!m_highlightNode)
        return nullptr;

    Node* node = m_highlightNode.get();
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nullptr;

    RefPtr<InspectorArray> highlightFragments = buildObjectForRendererFragments(renderer, m_nodeHighlightConfig);
    if (!highlightFragments)
        return nullptr;

    RefPtr<InspectorObject> highlightObject = InspectorObject::create();

    // The main view's scroll offset is shared across all quads.
    FrameView* mainView = m_page.mainFrame().view();
    highlightObject->setObject("scroll", buildObjectForPoint(!mainView->delegatesScrolling() ? mainView->visibleContentRect().location() : FloatPoint()));

    highlightObject->setArray("fragments", highlightFragments.release());

    if (m_nodeHighlightConfig.showInfo) {
        RefPtr<InspectorObject> elementInfo = buildObjectForElementInfo(node);
        if (elementInfo)
            highlightObject->setObject("elementInfo", elementInfo.release());
    }
        
    return highlightObject.release();
}

void InspectorOverlay::drawNodeHighlight()
{
    RefPtr<InspectorObject> highlightObject = buildObjectForHighlightedNode();
    if (!highlightObject)
        return;
    evaluateInOverlay("drawNodeHighlight", highlightObject);
}

void InspectorOverlay::drawQuadHighlight()
{
    if (!m_highlightQuad)
        return;

    Highlight highlight;
    buildQuadHighlight(*m_highlightQuad, m_quadHighlightConfig, &highlight);
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

    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_overlayPage = adoptPtr(new Page(pageClients));

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

void InspectorOverlay::reset(const IntSize& viewportSize, const IntSize& frameViewFullSize)
{
    RefPtr<InspectorObject> resetData = InspectorObject::create();
    resetData->setNumber("deviceScaleFactor", m_page.deviceScaleFactor());
    resetData->setObject("viewportSize", buildObjectForSize(viewportSize));
    resetData->setObject("frameViewFullSize", buildObjectForSize(frameViewFullSize));
    evaluateInOverlay("reset", resetData.release());
}

void InspectorOverlay::evaluateInOverlay(const String& method, const String& argument)
{
    RefPtr<InspectorArray> command = InspectorArray::create();
    command->pushString(method);
    command->pushString(argument);
    overlayPage()->mainFrame().script().evaluate(ScriptSourceCode(makeString("dispatch(", command->toJSONString(), ")")));
}

void InspectorOverlay::evaluateInOverlay(const String& method, PassRefPtr<InspectorValue> argument)
{
    RefPtr<InspectorArray> command = InspectorArray::create();
    command->pushString(method);
    command->pushValue(argument);
    overlayPage()->mainFrame().script().evaluate(ScriptSourceCode(makeString("dispatch(", command->toJSONString(), ")")));
}

void InspectorOverlay::freePage()
{
    m_overlayPage.clear();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
