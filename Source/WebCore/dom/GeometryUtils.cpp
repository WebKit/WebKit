/*
 * Copyright (C) 2026 Jochen Kühner (jochen.kuehner@gmx.de)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "config.h"
#include "GeometryUtils.h"

#include "ContainerNodeInlines.h"
#include "DOMPoint.h"
#include "DOMQuad.h"
#include "DOMRectReadOnly.h"
#include "Document.h"
#include "Element.h"
#include "FloatQuad.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Node.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderInline.h"
#include "RenderObjectInlines.h"
#include "RenderObjectStyle.h"
#include "RenderText.h"
#include "RenderView.h"
#include "SecurityOrigin.h"
#include "Text.h"
#include <wtf/CheckedPtr.h>
#include <wtf/TypeCasts.h>

namespace WebCore::GeometryUtils {

static Node& nodeForGeometryNode(const GeometryNode& geometryNode)
{
    return WTF::switchOn(geometryNode, [](const auto& node) -> Node& {
        return node.get();
    });
}

static RenderObject* rendererForNode(Node& node)
{
    if (auto* document = dynamicDowncast<Document>(node))
        return document->renderView();
    if (auto* element = dynamicDowncast<Element>(node))
        return element->renderer();
    if (auto* text = dynamicDowncast<Text>(node))
        return text->renderer();
    return nullptr;
}

// Text nodes use their parent element's coordinate system for conversion,
// while their own renderer supplies the fragments returned by getBoxQuads().
static RenderObject* coordinateRendererForNode(Node& node)
{
    if (auto* text = dynamicDowncast<Text>(node)) {
        if (RefPtr parent = text->parentElement())
            return parent->renderer();
    }
    return rendererForNode(node);
}

static RefPtr<LocalFrame> sameOriginRoot(Document& document)
{
    RefPtr frame = document.frame();
    if (!frame)
        return nullptr;

    while (RefPtr parentFrame = frame->tree().parent()) {
        RefPtr localParent = dynamicDowncast<LocalFrame>(parentFrame.get());
        if (!localParent)
            return nullptr;
        RefPtr childDocument = frame->document();
        RefPtr parentDocument = localParent->document();
        if (!childDocument || !parentDocument)
            return nullptr;
        if (!childDocument->securityOrigin().isSameOriginDomain(parentDocument->securityOrigin()))
            return nullptr;
        frame = WTF::move(localParent);
    }
    return frame;
}

static bool canMapBetween(Document& source, Document& target)
{
    if (&source == &target)
        return true;
    RefPtr sourceRoot = sameOriginRoot(source);
    return sourceRoot && sourceRoot == sameOriginRoot(target);
}

static Element* layoutUpdateContext(Node& node)
{
    if (auto* element = dynamicDowncast<Element>(node))
        return element;
    if (auto* text = dynamicDowncast<Text>(node))
        return text->parentElement();
    if (auto* document = dynamicDowncast<Document>(node))
        return document->documentElement();
    return nullptr;
}

static void updateLayoutForGeometryNode(Node& node)
{
    auto options = OptionSet<LayoutOptions> { LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible, LayoutOptions::CanDeferUpdateLayerPositions, LayoutOptions::IgnorePendingStylesheets };
    if (auto* context = layoutUpdateContext(node)) {
        node.document().updateLayoutIfDimensionsOutOfDate(*context, { DimensionsCheck::Left, DimensionsCheck::Top, DimensionsCheck::Width, DimensionsCheck::Height, DimensionsCheck::IgnoreOverflow }, options);
        return;
    }
    node.document().updateLayoutIgnorePendingStylesheets(options);
}

static void updateLayoutForGeometryNodes(Node& source, Node* target)
{
    updateLayoutForGeometryNode(source);
    if (target && target != &source && !target->isShadowIncludingInclusiveAncestorOf(source))
        updateLayoutForGeometryNode(*target);
}

static LayoutRect boxRect(const RenderObject& renderer, GeometryBox boxType)
{
    if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer)) {
        switch (boxType) {
        case GeometryBox::Margin: {
            auto horizontalMargins = box->marginLeft() + box->marginRight();
            auto verticalMargins = box->marginTop() + box->marginBottom();
            return { -box->marginLeft(), -box->marginTop(), box->borderBoxWidth() + horizontalMargins, box->borderBoxHeight() + verticalMargins };
        }
        case GeometryBox::Border:
            return box->borderBoxRect();
        case GeometryBox::Padding:
            return box->paddingBoxRect();
        case GeometryBox::Content:
            return box->contentBoxRect();
        }
    }

    if (CheckedPtr inlineRenderer = dynamicDowncast<RenderInline>(renderer)) {
        Vector<LayoutRect> lineBoxes;
        inlineRenderer->collectLineBoxRects(lineBoxes, { });
        if (!lineBoxes.isEmpty())
            return lineBoxes[0];
    }

    return { };
}

static Vector<FloatQuad> absoluteBoxQuads(RenderObject& renderer, GeometryBox boxType)
{
    Vector<FloatQuad> quads;

    // RenderObject::absoluteQuads() knows how to enumerate line boxes, text
    // runs, SVG boxes, and fragmented border boxes.
    if (boxType == GeometryBox::Border || !is<RenderBox>(renderer)) {
        renderer.absoluteQuads(quads);
        return quads;
    }

    quads.append(renderer.localToAbsoluteQuad(FloatQuad { FloatRect { boxRect(renderer, boxType) } }));
    return quads;
}

static FloatQuad mapAbsoluteBetweenDocuments(FloatQuad quad, Document& source, Document& target)
{
    if (&source == &target)
        return quad;
    RefPtr sourceView = source.view();
    RefPtr targetView = target.view();
    ASSERT(sourceView && targetView);
    return targetView->rootViewToContents(sourceView->contentsToRootView(quad));
}

static FloatQuad clientToAbsolute(FloatQuad quad, Document& document, const Style::ComputedStyle& style)
{
    RefPtr view = document.view();
    if (!view)
        return quad;
    auto offset = view->documentToClientOffset();
    quad.move(-offset.width(), -offset.height());
    float scale = view->absoluteToDocumentScaleFactor(document.zoomForClient(style));
    if (scale && scale != 1)
        quad.scale(1 / scale);
    return quad;
}

static FloatQuad absoluteToClient(FloatQuad quad, Document& document, const Style::ComputedStyle& sourceStyle)
{
    Vector<FloatQuad> quads;
    quads.append(quad);
    document.convertAbsoluteToClientQuads(quads, sourceStyle);
    return quads[0];
}

static FloatQuad mapAbsoluteToTarget(FloatQuad quad, Document& sourceDocument, Node& target, const Style::ComputedStyle& sourceStyle)
{
    Document& targetDocument = target.document();
    quad = mapAbsoluteBetweenDocuments(quad, sourceDocument, targetDocument);

    if (is<Document>(target))
        return absoluteToClient(quad, targetDocument, sourceStyle);

    auto* targetRenderer = coordinateRendererForNode(target);
    ASSERT(targetRenderer);
    quad = targetRenderer->absoluteToLocalQuad(quad);
    auto origin = boxRect(*targetRenderer, GeometryBox::Border).location();
    quad.move(-origin.x(), -origin.y());
    float zoom = targetRenderer->style().usedZoom();
    if (zoom && zoom != 1)
        quad.scale(1 / zoom);
    return quad;
}

static Ref<DOMQuad> toDOMQuad(const FloatQuad& quad)
{
    auto point = [](const FloatPoint& point) {
        return DOMPointInit { point.x(), point.y(), 0, 1 };
    };
    return DOMQuad::create(point(quad.p1()), point(quad.p2()), point(quad.p3()), point(quad.p4()));
}

static FloatQuad fromDOMQuad(const DOMQuadInit& quad)
{
    auto point = [](const DOMPointInit& point) {
        return FloatPoint { narrowPrecisionToFloat(point.x), narrowPrecisionToFloat(point.y) };
    };
    return { point(quad.p1), point(quad.p2), point(quad.p3), point(quad.p4) };
}

ExceptionOr<Vector<Ref<DOMQuad>>> getBoxQuads(Node& source, BoxQuadOptions&& options)
{
    Document& sourceDocument = source.document();
    Node* target = options.relativeTo ? &nodeForGeometryNode(*options.relativeTo) : &sourceDocument;
    Document& targetDocument = target->document();

    updateLayoutForGeometryNodes(source, target);

    if (!canMapBetween(sourceDocument, targetDocument))
        return Exception { ExceptionCode::SecurityError };

    auto* targetRenderer = coordinateRendererForNode(*target);
    if (!is<Document>(*target) && !targetRenderer)
        return Exception { ExceptionCode::NotFoundError };

    Vector<FloatQuad> sourceQuads;
    RenderObject* sourceRenderer = rendererForNode(source);
    if (is<Document>(source)) {
        RefPtr view = sourceDocument.view();
        if (!view || !sourceRenderer)
            return Vector<Ref<DOMQuad>> { };
        auto viewportSize = view->layoutViewportRect().size();
        FloatQuad viewportQuad { FloatRect { { }, FloatSize { viewportSize } } };
        sourceQuads.append(clientToAbsolute(viewportQuad, sourceDocument, sourceRenderer->style()));
    } else {
        if (!sourceRenderer)
            return Vector<Ref<DOMQuad>> { };
        sourceQuads = absoluteBoxQuads(*sourceRenderer, options.box);
    }

    Vector<Ref<DOMQuad>> result;
    result.reserveInitialCapacity(sourceQuads.size());
    for (auto& quad : sourceQuads)
        result.append(toDOMQuad(mapAbsoluteToTarget(quad, sourceDocument, *target, sourceRenderer->style())));
    return result;
}

static ExceptionOr<FloatQuad> convertQuad(Node& target, FloatQuad quad, Node& source, const ConvertCoordinateOptions& options)
{
    Document& sourceDocument = source.document();
    Document& targetDocument = target.document();
    updateLayoutForGeometryNodes(source, &target);

    if (!canMapBetween(sourceDocument, targetDocument))
        return Exception { ExceptionCode::SecurityError };

    RenderObject* sourceRenderer = coordinateRendererForNode(source);
    RenderObject* targetRenderer = coordinateRendererForNode(target);
    if (!sourceRenderer || !targetRenderer)
        return Exception { ExceptionCode::NotFoundError };

    if (is<Document>(source))
        quad = clientToAbsolute(quad, sourceDocument, sourceRenderer->style());
    else {
        float zoom = sourceRenderer->style().usedZoom();
        if (zoom && zoom != 1)
            quad.scale(zoom);
        auto origin = boxRect(*sourceRenderer, options.fromBox).location();
        quad.move(origin.x(), origin.y());
        quad = sourceRenderer->localToAbsoluteQuad(quad);
    }

    quad = mapAbsoluteBetweenDocuments(quad, sourceDocument, targetDocument);
    if (is<Document>(target))
        return absoluteToClient(quad, targetDocument, sourceRenderer->style());

    quad = targetRenderer->absoluteToLocalQuad(quad);
    auto targetOrigin = boxRect(*targetRenderer, options.toBox).location();
    quad.move(-targetOrigin.x(), -targetOrigin.y());
    float targetZoom = targetRenderer->style().usedZoom();
    if (targetZoom && targetZoom != 1)
        quad.scale(1 / targetZoom);
    return quad;
}

ExceptionOr<Ref<DOMQuad>> convertQuadFromNode(Node& target, DOMQuadInit&& quad, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    auto converted = convertQuad(target, fromDOMQuad(quad), nodeForGeometryNode(from), options);
    if (converted.hasException())
        return converted.releaseException();
    return toDOMQuad(converted.releaseReturnValue());
}

ExceptionOr<Ref<DOMQuad>> convertRectFromNode(Node& target, DOMRectReadOnly& rect, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    FloatQuad quad { rect.toFloatRect() };
    auto converted = convertQuad(target, quad, nodeForGeometryNode(from), options);
    if (converted.hasException())
        return converted.releaseException();
    return toDOMQuad(converted.releaseReturnValue());
}

ExceptionOr<Ref<DOMPoint>> convertPointFromNode(Node& target, DOMPointInit&& point, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    FloatPoint floatPoint { narrowPrecisionToFloat(point.x), narrowPrecisionToFloat(point.y) };
    auto converted = convertQuad(target, FloatQuad { floatPoint, floatPoint, floatPoint, floatPoint }, nodeForGeometryNode(from), options);
    if (converted.hasException())
        return converted.releaseException();
    auto result = converted.releaseReturnValue().p1();
    return DOMPoint::create(result.x(), result.y(), 0, 1);
}

} // namespace WebCore::GeometryUtils

namespace WebCore {

ExceptionOr<Vector<Ref<DOMQuad>>> Element::getBoxQuads(BoxQuadOptions&& options)
{
    return GeometryUtils::getBoxQuads(*this, WTF::move(options));
}

ExceptionOr<Ref<DOMQuad>> Element::convertQuadFromNode(DOMQuadInit&& quad, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    return GeometryUtils::convertQuadFromNode(*this, WTF::move(quad), WTF::move(from), WTF::move(options));
}

ExceptionOr<Ref<DOMQuad>> Element::convertRectFromNode(DOMRectReadOnly& rect, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    return GeometryUtils::convertRectFromNode(*this, rect, WTF::move(from), WTF::move(options));
}

ExceptionOr<Ref<DOMPoint>> Element::convertPointFromNode(DOMPointInit&& point, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    return GeometryUtils::convertPointFromNode(*this, WTF::move(point), WTF::move(from), WTF::move(options));
}

ExceptionOr<Vector<Ref<DOMQuad>>> Text::getBoxQuads(BoxQuadOptions&& options)
{
    return GeometryUtils::getBoxQuads(*this, WTF::move(options));
}

ExceptionOr<Ref<DOMQuad>> Text::convertQuadFromNode(DOMQuadInit&& quad, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    return GeometryUtils::convertQuadFromNode(*this, WTF::move(quad), WTF::move(from), WTF::move(options));
}

ExceptionOr<Ref<DOMQuad>> Text::convertRectFromNode(DOMRectReadOnly& rect, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    return GeometryUtils::convertRectFromNode(*this, rect, WTF::move(from), WTF::move(options));
}

ExceptionOr<Ref<DOMPoint>> Text::convertPointFromNode(DOMPointInit&& point, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    return GeometryUtils::convertPointFromNode(*this, WTF::move(point), WTF::move(from), WTF::move(options));
}

ExceptionOr<Vector<Ref<DOMQuad>>> Document::getBoxQuads(BoxQuadOptions&& options)
{
    return GeometryUtils::getBoxQuads(*this, WTF::move(options));
}

ExceptionOr<Ref<DOMQuad>> Document::convertQuadFromNode(DOMQuadInit&& quad, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    return GeometryUtils::convertQuadFromNode(*this, WTF::move(quad), WTF::move(from), WTF::move(options));
}

ExceptionOr<Ref<DOMQuad>> Document::convertRectFromNode(DOMRectReadOnly& rect, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    return GeometryUtils::convertRectFromNode(*this, rect, WTF::move(from), WTF::move(options));
}

ExceptionOr<Ref<DOMPoint>> Document::convertPointFromNode(DOMPointInit&& point, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    return GeometryUtils::convertPointFromNode(*this, WTF::move(point), WTF::move(from), WTF::move(options));
}

} // namespace WebCore
