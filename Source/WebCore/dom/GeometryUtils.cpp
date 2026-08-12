/*
 * Copyright (C) 2026 Jochen Kühner (jochen.kuehner@gmx.de)
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

static Ref<Node> nodeForGeometryNode(const GeometryNode& geometryNode)
{
    return WTF::switchOn(geometryNode, [](const auto& node) -> Ref<Node> {
        return node;
    });
}

static CheckedPtr<RenderObject> rendererForNode(Node& node)
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
static CheckedPtr<RenderObject> coordinateRendererForNode(Node& node)
{
    if (RefPtr text = dynamicDowncast<Text>(node)) {
        if (RefPtr parent = text->parentElement())
            return parent->renderer();
    }
    return rendererForNode(node);
}

static RefPtr<LocalFrame> sameOriginRoot(Document& document)
{
    Ref protectedDocument = document;
    RefPtr frame = protectedDocument->frame();
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
        Ref childOrigin = childDocument->securityOrigin();
        Ref parentOrigin = parentDocument->securityOrigin();
        if (!childOrigin->isSameOriginDomain(parentOrigin))
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

static RefPtr<Element> layoutUpdateContext(Node& node)
{
    if (auto* element = dynamicDowncast<Element>(node))
        return element;
    if (auto* text = dynamicDowncast<Text>(node))
        return text->parentElement();
    if (auto* document = dynamicDowncast<Document>(node))
        return document->documentElement();
    return nullptr;
}

static void updateLayoutForGeometryNode(Ref<Node> node)
{
    auto options = OptionSet<LayoutOptions> { LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible, LayoutOptions::CanDeferUpdateLayerPositions, LayoutOptions::IgnorePendingStylesheets };
    Ref document = node->document();
    if (RefPtr context = layoutUpdateContext(node)) {
        document->updateLayoutIfDimensionsOutOfDate(*context, { DimensionsCheck::Left, DimensionsCheck::Top, DimensionsCheck::Width, DimensionsCheck::Height, DimensionsCheck::IgnoreOverflow }, options);
        return;
    }
    document->updateLayoutIgnorePendingStylesheets(options);
}

static void updateLayoutForGeometryNodes(Ref<Node> source, RefPtr<Node> target)
{
    updateLayoutForGeometryNode(source.copyRef());
    if (target && target != source.ptr() && !target->isShadowIncludingInclusiveAncestorOf(source))
        updateLayoutForGeometryNode(target.releaseNonNull());
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
    Ref protectedTarget = target;
    Ref targetDocument = protectedTarget->document();
    quad = mapAbsoluteBetweenDocuments(quad, sourceDocument, targetDocument);

    if (is<Document>(protectedTarget.get()))
        return absoluteToClient(quad, targetDocument, sourceStyle);

    CheckedPtr targetRenderer = coordinateRendererForNode(protectedTarget);
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
    Ref protectedSource = source;
    Ref sourceDocument = protectedSource->document();
    Ref<Node> target = options.relativeTo ? nodeForGeometryNode(*options.relativeTo) : Ref<Node> { sourceDocument.get() };
    Ref targetDocument = target->document();

    updateLayoutForGeometryNodes(protectedSource.copyRef(), target.copyRef());

    if (!canMapBetween(sourceDocument, targetDocument))
        return Exception { ExceptionCode::SecurityError };

    CheckedPtr targetRenderer = coordinateRendererForNode(target);
    if (!is<Document>(target.get()) && !targetRenderer)
        return Exception { ExceptionCode::NotFoundError };

    Vector<FloatQuad> sourceQuads;
    CheckedPtr sourceRenderer = rendererForNode(protectedSource);
    if (!sourceRenderer)
        return Vector<Ref<DOMQuad>> { };
    CheckedRef sourceStyle = sourceRenderer->style();
    if (is<Document>(protectedSource.get())) {
        RefPtr view = sourceDocument->view();
        if (!view)
            return Vector<Ref<DOMQuad>> { };
        auto viewportSize = view->layoutViewportRect().size();
        FloatQuad viewportQuad { FloatRect { { }, FloatSize { viewportSize } } };
        sourceQuads.append(clientToAbsolute(viewportQuad, sourceDocument, sourceStyle));
    } else
        sourceQuads = absoluteBoxQuads(*sourceRenderer, options.box);

    Vector<Ref<DOMQuad>> result;
    result.reserveInitialCapacity(sourceQuads.size());
    for (auto& quad : sourceQuads)
        result.append(toDOMQuad(mapAbsoluteToTarget(quad, sourceDocument, target, sourceStyle)));
    return result;
}

static ExceptionOr<FloatQuad> convertQuad(Ref<Node> target, FloatQuad quad, Ref<Node> source, const ConvertCoordinateOptions& options)
{
    Ref sourceDocument = source->document();
    Ref targetDocument = target->document();
    updateLayoutForGeometryNodes(source.copyRef(), target.copyRef());

    if (!canMapBetween(sourceDocument, targetDocument))
        return Exception { ExceptionCode::SecurityError };

    CheckedPtr sourceRenderer = coordinateRendererForNode(source);
    CheckedPtr targetRenderer = coordinateRendererForNode(target);
    if (!sourceRenderer || !targetRenderer)
        return Exception { ExceptionCode::NotFoundError };

    CheckedRef sourceStyle = sourceRenderer->style();
    if (is<Document>(source.get()))
        quad = clientToAbsolute(quad, sourceDocument, sourceStyle);
    else {
        float zoom = sourceRenderer->style().usedZoom();
        if (zoom && zoom != 1)
            quad.scale(zoom);
        auto origin = boxRect(*sourceRenderer, options.fromBox).location();
        quad.move(origin.x(), origin.y());
        quad = sourceRenderer->localToAbsoluteQuad(quad);
    }

    quad = mapAbsoluteBetweenDocuments(quad, sourceDocument, targetDocument);
    if (is<Document>(target.get()))
        return absoluteToClient(quad, targetDocument, sourceStyle);

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
    auto converted = convertQuad(Ref { target }, fromDOMQuad(quad), nodeForGeometryNode(from), options);
    if (converted.hasException())
        return converted.releaseException();
    return toDOMQuad(converted.releaseReturnValue());
}

ExceptionOr<Ref<DOMQuad>> convertRectFromNode(Node& target, DOMRectReadOnly& rect, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    FloatQuad quad { rect.toFloatRect() };
    auto converted = convertQuad(Ref { target }, quad, nodeForGeometryNode(from), options);
    if (converted.hasException())
        return converted.releaseException();
    return toDOMQuad(converted.releaseReturnValue());
}

ExceptionOr<Ref<DOMPoint>> convertPointFromNode(Node& target, DOMPointInit&& point, GeometryNode&& from, ConvertCoordinateOptions&& options)
{
    FloatPoint floatPoint { narrowPrecisionToFloat(point.x), narrowPrecisionToFloat(point.y) };
    auto converted = convertQuad(Ref { target }, FloatQuad { floatPoint, floatPoint, floatPoint, floatPoint }, nodeForGeometryNode(from), options);
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
