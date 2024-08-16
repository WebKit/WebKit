/*
 * Copyright (C) 2004, 2005, 2006, 2019 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
 */

#include "config.h"
#include "SVGSVGElement.h"

#include "CSSHelper.h"
#include "DOMMatrix2DInit.h"
#include "DOMWrapperWorld.h"
#include "ElementIterator.h"
#include "EventNames.h"
#include "FrameSelection.h"
#include "LegacyRenderSVGResource.h"
#include "LegacyRenderSVGRoot.h"
#include "LegacyRenderSVGViewportContainer.h"
#include "LocalFrame.h"
#include "NodeName.h"
#include "RenderBoxInlines.h"
#include "RenderSVGRoot.h"
#include "RenderSVGViewportContainer.h"
#include "RenderView.h"
#include "SMILTimeContainer.h"
#include "SVGAngle.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGLength.h"
#include "SVGMatrix.h"
#include "SVGNumber.h"
#include "SVGPoint.h"
#include "SVGRect.h"
#include "SVGTransform.h"
#include "SVGViewElement.h"
#include "SVGViewSpec.h"
#include "StaticNodeList.h"
#include "TreeScopeInlines.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGSVGElement);

inline SVGSVGElement::SVGSVGElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this), TypeFlag::HasDidMoveToNewDocument)
    , SVGFitToViewBox(this)
    , m_timeContainer(SMILTimeContainer::create(*this))
{
    ASSERT(hasTagName(SVGNames::svgTag));
    document.registerForDocumentSuspensionCallbacks(*this);

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGSVGElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGSVGElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGSVGElement::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGSVGElement::m_height>();
    });
}

Ref<SVGSVGElement> SVGSVGElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGSVGElement(tagName, document));
}

Ref<SVGSVGElement> SVGSVGElement::create(Document& document)
{
    return create(SVGNames::svgTag, document);
}

SVGSVGElement::~SVGSVGElement()
{
    if (RefPtr viewSpec = m_viewSpec)
        viewSpec->resetContextElement();
    RefAllowingPartiallyDestroyed<Document> document = this->document();
    document->unregisterForDocumentSuspensionCallbacks(*this);
    document->checkedSVGExtensions()->removeTimeContainer(*this);
}

void SVGSVGElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    oldDocument.unregisterForDocumentSuspensionCallbacks(*this);
    protectedDocument()->registerForDocumentSuspensionCallbacks(*this);
    SVGGraphicsElement::didMoveToNewDocument(oldDocument, newDocument);
}

SVGViewSpec& SVGSVGElement::currentView()
{
    if (!m_viewSpec)
        m_viewSpec = SVGViewSpec::create(*this);
    return *m_viewSpec;
}

RefPtr<LocalFrame> SVGSVGElement::frameForCurrentScale() const
{
    // The behavior of currentScale() is undefined when we're dealing with non-standalone SVG documents.
    // If the document is embedded, the scaling is handled by the host renderer.
    if (!isConnected() || !isOutermostSVGSVGElement() || parentNode())
        return nullptr;
    RefPtr frame = document().frame();
    return frame && frame->isMainFrame() ? frame : nullptr;
}

float SVGSVGElement::currentScale() const
{
    // When asking from inside an embedded SVG document, a scale value of 1 seems reasonable, as it doesn't
    // know anything about the parent scale.
    auto frame = frameForCurrentScale();
    return frame ? frame->pageZoomFactor() : 1;
}

void SVGSVGElement::setCurrentScale(float scale)
{
    ASSERT(std::isfinite(scale));
    if (auto frame = frameForCurrentScale())
        frame->setPageZoomFactor(scale);
}

void SVGSVGElement::setCurrentTranslate(const FloatPoint& translation)
{
    if (m_currentTranslate->value() == translation)
        return;
    m_currentTranslate->setValue(translation);
    updateCurrentTranslate();
}

void SVGSVGElement::updateCurrentTranslate()
{
    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return;

    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (CheckedPtr svgRoot = dynamicDowncast<RenderSVGRoot>(*renderer)) {
            ASSERT(svgRoot->viewportContainer());
            svgRoot->checkedViewportContainer()->updateHasSVGTransformFlags();
        }

        // TODO: [LBSE] Avoid relayout upon transform changes (not possible in legacy, but should be in LBSE).
        updateSVGRendererForElementChange();
        return;
    }

    updateSVGRendererForElementChange();
    if (parentNode() == &document() && document().renderView())
        protectedDocument()->checkedRenderView()->repaint();
}

void SVGSVGElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (!nearestViewportElement() && isConnected()) {
        // For these events, the outermost <svg> element works like a <body> element does,
        // setting certain event handlers directly on the window object.
        switch (name.nodeName()) {
        case AttributeNames::onunloadAttr:
            protectedDocument()->setWindowAttributeEventListener(eventNames().unloadEvent, name, newValue, protectedMainThreadNormalWorld());
            return;
        case AttributeNames::onresizeAttr:
            protectedDocument()->setWindowAttributeEventListener(eventNames().resizeEvent, name, newValue, protectedMainThreadNormalWorld());
            return;
        case AttributeNames::onscrollAttr:
            protectedDocument()->setWindowAttributeEventListener(eventNames().scrollEvent, name, newValue, protectedMainThreadNormalWorld());
            return;
        case AttributeNames::onzoomAttr:
            protectedDocument()->setWindowAttributeEventListener(eventNames().zoomEvent, name, newValue, protectedMainThreadNormalWorld());
            return;
        case AttributeNames::onabortAttr:
            protectedDocument()->setWindowAttributeEventListener(eventNames().abortEvent, name, newValue, protectedMainThreadNormalWorld());
            return;
        case AttributeNames::onerrorAttr:
            protectedDocument()->setWindowAttributeEventListener(eventNames().errorEvent, name, newValue, protectedMainThreadNormalWorld());
            return;
        default:
            break;
        }
    }

    SVGParsingError parseError = NoError;

    switch (name.nodeName()) {
    case AttributeNames::xAttr:
        Ref { m_x }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::yAttr:
        Ref { m_y }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::widthAttr: {
        auto length = SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Forbid);
        if (parseError != NoError || newValue.isEmpty()) {
            // FIXME: This is definitely the correct behavior for a missing/removed attribute.
            // Not sure it's correct for the empty string or for something that can't be parsed.
            length = SVGLengthValue(SVGLengthMode::Width, "100%"_s);
        }
        Ref { m_width }->setBaseValInternal(length);
        break;
    }
    case AttributeNames::heightAttr: {
        auto length = SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Forbid);
        if (parseError != NoError || newValue.isEmpty()) {
            // FIXME: This is definitely the correct behavior for a removed attribute.
            // Not sure it's correct for the empty string or for something that can't be parsed.
            length = SVGLengthValue(SVGLengthMode::Height, "100%"_s);
        }
        Ref { m_height }->setBaseValInternal(length);
        break;
    }
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGFitToViewBox::parseAttribute(name, newValue);
    SVGZoomAndPan::parseAttribute(name, newValue);
    SVGGraphicsElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGSVGElement::svgAttributeChanged(const QualifiedName& attrName)
{
    auto isEmbeddedThroughFrameContainingSVGDocument = [](const RenderElement& renderer) -> bool {
        if (CheckedPtr svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(renderer))
            return svgRoot->isEmbeddedThroughFrameContainingSVGDocument();

        if (CheckedPtr svgRoot = dynamicDowncast<RenderSVGRoot>(renderer))
            return svgRoot->isEmbeddedThroughFrameContainingSVGDocument();

        return false;
    };

    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        setPresentationalHintStyleIsDirty();

        if (attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr) {
            // FIXME: try to get rid of this custom handling of embedded SVG invalidation, maybe through abstraction.
            if (CheckedPtr renderer = this->renderer()) {
                if (isEmbeddedThroughFrameContainingSVGDocument(*renderer))
                    renderer->checkedView()->setNeedsLayout(MarkOnlyThis);
            }
        }
        invalidateResourceImageBuffersIfNeeded();
        updateSVGRendererForElementChange();
        return;
    }

    if (SVGFitToViewBox::isKnownAttribute(attrName)) {
        if (document().settings().layerBasedSVGEngineEnabled()) {
            if (CheckedPtr svgRoot = dynamicDowncast<RenderSVGRoot>(renderer())) {
                ASSERT(svgRoot->viewportContainer());
                svgRoot->checkedViewportContainer()->updateHasSVGTransformFlags();
            } else if (CheckedPtr viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(renderer()))
                viewportContainer->updateHasSVGTransformFlags();

            // TODO: [LBSE] Avoid relayout upon transform changes (not possible in legacy, but should be in LBSE).
            updateSVGRendererForElementChange();
            return;
        }

        if (CheckedPtr renderer = this->renderer())
            renderer->setNeedsTransformUpdate();

        invalidateResourceImageBuffersIfNeeded();
        updateSVGRendererForElementChange();
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

Ref<NodeList> SVGSVGElement::collectIntersectionOrEnclosureList(SVGRect& rect, SVGElement* referenceElement, bool (*checkFunction)(SVGElement&, SVGRect&))
{
    Vector<Ref<Element>> elements;
    for (Ref element : descendantsOfType<SVGElement>(referenceElement ? *referenceElement : *this)) {
        if (checkFunction(element, rect))
            elements.append(WTFMove(element));
    }
    return StaticElementList::create(WTFMove(elements));
}

static bool checkIntersectionWithoutUpdatingLayout(SVGElement& element, SVGRect& rect)
{
    if (element.document().settings().layerBasedSVGEngineEnabled())
        return RenderSVGModelObject::checkIntersection(element.checkedRenderer().get(), rect.value());
    return LegacyRenderSVGModelObject::checkIntersection(element.checkedRenderer().get(), rect.value());
}
    
static bool checkEnclosureWithoutUpdatingLayout(SVGElement& element, SVGRect& rect)
{
    if (element.document().settings().layerBasedSVGEngineEnabled())
        return RenderSVGModelObject::checkEnclosure(element.checkedRenderer().get(), rect.value());
    return LegacyRenderSVGModelObject::checkEnclosure(element.checkedRenderer().get(), rect.value());
}

Ref<NodeList> SVGSVGElement::getIntersectionList(SVGRect& rect, SVGElement* referenceElement)
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);
    return collectIntersectionOrEnclosureList(rect, referenceElement, checkIntersectionWithoutUpdatingLayout);
}

Ref<NodeList> SVGSVGElement::getEnclosureList(SVGRect& rect, SVGElement* referenceElement)
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);
    return collectIntersectionOrEnclosureList(rect, referenceElement, checkEnclosureWithoutUpdatingLayout);
}

bool SVGSVGElement::checkIntersection(Ref<SVGElement>&& element, SVGRect& rect)
{
    element->protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, element.ptr());
    return checkIntersectionWithoutUpdatingLayout(element, rect);
}

bool SVGSVGElement::checkEnclosure(Ref<SVGElement>&& element, SVGRect& rect)
{
    element->protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, element.ptr());
    return checkEnclosureWithoutUpdatingLayout(element, rect);
}

void SVGSVGElement::deselectAll()
{
    if (RefPtr frame = document().frame())
        frame->checkedSelection()->clear();
}

Ref<SVGNumber> SVGSVGElement::createSVGNumber()
{
    return SVGNumber::create();
}

Ref<SVGLength> SVGSVGElement::createSVGLength()
{
    return SVGLength::create();
}

Ref<SVGAngle> SVGSVGElement::createSVGAngle()
{
    return SVGAngle::create();
}

Ref<SVGPoint> SVGSVGElement::createSVGPoint()
{
    return SVGPoint::create();
}

Ref<SVGMatrix> SVGSVGElement::createSVGMatrix()
{
    return SVGMatrix::create();
}

Ref<SVGRect> SVGSVGElement::createSVGRect()
{
    return SVGRect::create();
}

Ref<SVGTransform> SVGSVGElement::createSVGTransform()
{
    return SVGTransform::create(SVGTransformValue::SVG_TRANSFORM_MATRIX);
}

Ref<SVGTransform> SVGSVGElement::createSVGTransformFromMatrix(DOMMatrix2DInit&& matrixInit)
{
    AffineTransform transform;
    if (matrixInit.a)
        transform.setA(*matrixInit.a);
    if (matrixInit.b)
        transform.setB(*matrixInit.b);
    if (matrixInit.c)
        transform.setC(*matrixInit.c);
    if (matrixInit.d)
        transform.setD(*matrixInit.d);
    if (matrixInit.e)
        transform.setE(*matrixInit.e);
    if (matrixInit.f)
        transform.setF(*matrixInit.f);
    return SVGTransform::create(transform);
}

AffineTransform SVGSVGElement::localCoordinateSpaceTransform(SVGLocatable::CTMScope mode) const
{
    AffineTransform viewBoxTransform;
    if (!hasEmptyViewBox()) {
        FloatSize size = currentViewportSizeExcludingZoom();
        viewBoxTransform = viewBoxToViewTransform(size.width(), size.height());
    }

    if (document().settings().layerBasedSVGEngineEnabled()) {
        // LBSE only uses this code path for operation on "detached" elements (no renderer).
        AffineTransform transform;
        if (!isOutermostSVGSVGElement()) {
            SVGLengthContext lengthContext(this);
            transform.translate(x().value(lengthContext), y().value(lengthContext));
        }

        if (viewBoxTransform.isIdentity())
            return transform;
        return transform.multiply(viewBoxTransform);
    }

    AffineTransform transform;
    if (!isOutermostSVGSVGElement()) {
        SVGLengthContext lengthContext(this);
        transform.translate(x().value(lengthContext), y().value(lengthContext));
    } else if (mode == SVGLocatable::ScreenScope) {
        if (CheckedPtr renderer = this->renderer()) {
            FloatPoint location;
            float zoomFactor = 1;

            // At the SVG/HTML boundary (aka LegacyRenderSVGRoot), we apply the localToBorderBoxTransform
            // to map an element from SVG viewport coordinates to CSS box coordinates.
            // LegacyRenderSVGRoot's localToAbsolute method expects CSS box coordinates.
            // We also need to adjust for the zoom level factored into CSS coordinates (bug #96361).
            if (CheckedPtr legacyRenderSVGRoot = dynamicDowncast<LegacyRenderSVGRoot>(*renderer)) {
                location = legacyRenderSVGRoot->localToBorderBoxTransform().mapPoint(location);
                zoomFactor = 1 / renderer->style().usedZoom();
            }

            // Translate in our CSS parent coordinate space
            // FIXME: This doesn't work correctly with CSS transforms.
            location = renderer->localToAbsolute(location, UseTransforms);
            location.scale(zoomFactor);

            // Be careful here! localToBorderBoxTransform() included the x/y offset coming from the viewBoxToViewTransform(),
            // so we have to subtract it here (original cause of bug #27183)
            transform.translate(location.x() - viewBoxTransform.e(), location.y() - viewBoxTransform.f());

            // Respect scroll offset.
            if (RefPtr view = document().view()) {
                LayoutPoint scrollPosition = view->scrollPosition();
                scrollPosition.scale(zoomFactor);
                transform.translate(-scrollPosition);
            }
        }
    }

    return transform.multiply(viewBoxTransform);
}

bool SVGSVGElement::rendererIsNeeded(const RenderStyle& style)
{
    if (!isValid())
        return false;
    // FIXME: We should respect display: none on the documentElement svg element
    // but many things in FrameView and SVGImage depend on the LegacyRenderSVGRoot when
    // they should instead depend on the RenderView.
    // https://bugs.webkit.org/show_bug.cgi?id=103493
    if (document().documentElement() == this)
        return true;
    return StyledElement::rendererIsNeeded(style);
}

RenderPtr<RenderElement> SVGSVGElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (isOutermostSVGSVGElement()) {
        if (document().settings().layerBasedSVGEngineEnabled()) {
            protectedDocument()->setMayHaveRenderedSVGRootElements();
            return createRenderer<RenderSVGRoot>(*this, WTFMove(style));
        }
        return createRenderer<LegacyRenderSVGRoot>(*this, WTFMove(style));
    }

    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGViewportContainer>(*this, WTFMove(style));
    return createRenderer<LegacyRenderSVGViewportContainer>(*this, WTFMove(style));
}

Node::InsertedIntoAncestorResult SVGSVGElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    if (insertionType.connectedToDocument) {
        Ref document = this->document();
        CheckedRef svgExtensions = document->svgExtensions();
        svgExtensions->addTimeContainer(*this);
        if (!svgExtensions->areAnimationsPaused())
            unpauseAnimations();

        // Animations are started at the end of document parsing and after firing the load event,
        // but if we miss that train (deferred programmatic element insertion for example) we need
        // to initialize the time container here.
        if (!document->parsing() && !document->processingLoadEvent() && document->loadEventFinished())
            m_timeContainer->begin();
    }
    return SVGGraphicsElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
}

void SVGSVGElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.disconnectedFromDocument) {
        RefAllowingPartiallyDestroyed<Document> document = this->document();
        document->checkedSVGExtensions()->removeTimeContainer(*this);
        pauseAnimations();
    }
    SVGGraphicsElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

void SVGSVGElement::pauseAnimations()
{
    Ref timeContainer = m_timeContainer;
    if (!timeContainer->isPaused())
        timeContainer->pause();
}

void SVGSVGElement::unpauseAnimations()
{
    Ref timeContainer = m_timeContainer;
    if (timeContainer->isPaused())
        timeContainer->resume();
}

bool SVGSVGElement::resumePausedAnimationsIfNeeded(const IntRect& visibleRect)
{
    bool animationEnabled = document().page() ? document().page()->imageAnimationEnabled() : true;
    if (!animationEnabled || !renderer() || !renderer()->isVisibleInDocumentRect(visibleRect))
        return false;

    unpauseAnimations();
    return true;
}

bool SVGSVGElement::animationsPaused() const
{
    return protectedTimeContainer()->isPaused();
}

bool SVGSVGElement::hasActiveAnimation() const
{
    return protectedTimeContainer()->isActive();
}

float SVGSVGElement::getCurrentTime() const
{
    return narrowPrecisionToFloat(protectedTimeContainer()->elapsed().value());
}

void SVGSVGElement::setCurrentTime(float seconds)
{
    ASSERT(std::isfinite(seconds));
    protectedTimeContainer()->setElapsed(std::max(seconds, 0.0f));
}

bool SVGSVGElement::selfHasRelativeLengths() const
{
    return x().isRelative()
        || y().isRelative()
        || width().isRelative()
        || height().isRelative()
        || hasAttribute(SVGNames::viewBoxAttr);
}

bool SVGSVGElement::hasTransformRelatedAttributes() const
{
    if (SVGGraphicsElement::hasTransformRelatedAttributes())
        return true;

    // 'x' / 'y' / 'viewBox' lead to a non-identity supplementalLayerTransform in RenderSVGViewportContainer
    return (hasAttribute(SVGNames::xAttr) || hasAttribute(SVGNames::yAttr)) || (hasAttribute(SVGNames::viewBoxAttr) && !hasEmptyViewBox());
}

FloatRect SVGSVGElement::currentViewBoxRect() const
{
    if (m_useCurrentView) {
        if (RefPtr viewSpec = m_viewSpec)
            return viewSpec->viewBox();
        return { };
    }

    auto viewBox = this->viewBox();
    if (!viewBox.isEmpty())
        return viewBox;

    auto isEmbeddedThroughSVGImage = [](const RenderElement* renderer) -> bool {
        if (!renderer)
            return false;

        if (auto* svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(renderer))
            return svgRoot->isEmbeddedThroughSVGImage();

        if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(renderer))
            return svgRoot->isEmbeddedThroughSVGImage();

        return false;
    };

    if (!isEmbeddedThroughSVGImage(checkedRenderer().get()))
        return { };

    auto intrinsicWidth = this->intrinsicWidth();
    auto intrinsicHeight = this->intrinsicHeight();
    if (!intrinsicWidth.isFixed() || !intrinsicHeight.isFixed())
        return { };

    // If no viewBox is specified but non-relative width/height values, then we
    // should always synthesize a viewBox if we're embedded through a SVGImage.
    return { 0, 0, floatValueForLength(intrinsicWidth, 0), floatValueForLength(intrinsicHeight, 0) };
}

FloatSize SVGSVGElement::currentViewportSizeExcludingZoom() const
{
    FloatSize viewportSize;

    if (renderer()) {
        if (CheckedPtr svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(renderer()))
            viewportSize = svgRoot->contentBoxRect().size() / svgRoot->style().usedZoom();
        else if (CheckedPtr svgViewportContainer = dynamicDowncast<LegacyRenderSVGViewportContainer>(renderer()))
            viewportSize = svgViewportContainer->viewport().size();
        else if (CheckedPtr svgRoot = dynamicDowncast<RenderSVGRoot>(renderer()))
            viewportSize = svgRoot->contentBoxRect().size() / svgRoot->style().usedZoom();
        else if (CheckedPtr svgViewportContainer = dynamicDowncast<RenderSVGViewportContainer>(renderer()))
            viewportSize = svgViewportContainer->viewport().size();
        else {
            ASSERT_NOT_REACHED();
            return { };
        }
    }

    if (!viewportSize.isEmpty())
        return viewportSize;

    if (!(hasIntrinsicWidth() && hasIntrinsicHeight()))
        return { };

    return FloatSize(floatValueForLength(intrinsicWidth(), 0), floatValueForLength(intrinsicHeight(), 0));
}

bool SVGSVGElement::hasIntrinsicWidth() const
{
    return width().lengthType() != SVGLengthType::Percentage;
}

bool SVGSVGElement::hasIntrinsicHeight() const
{
    return height().lengthType() != SVGLengthType::Percentage;
}

Length SVGSVGElement::intrinsicWidth() const
{
    if (width().lengthType() == SVGLengthType::Percentage)
        return Length(0, LengthType::Fixed);

    SVGLengthContext lengthContext(this);
    return Length(width().value(lengthContext), LengthType::Fixed);
}

Length SVGSVGElement::intrinsicHeight() const
{
    if (height().lengthType() == SVGLengthType::Percentage)
        return Length(0, LengthType::Fixed);

    SVGLengthContext lengthContext(this);
    return Length(height().value(lengthContext), LengthType::Fixed);
}

AffineTransform SVGSVGElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    if (!m_useCurrentView || !m_viewSpec)
        return SVGFitToViewBox::viewBoxToViewTransform(currentViewBoxRect(), preserveAspectRatio(), viewWidth, viewHeight);

    RefPtr viewSpec = m_viewSpec;
    AffineTransform transform = SVGFitToViewBox::viewBoxToViewTransform(currentViewBoxRect(), viewSpec->preserveAspectRatio(), viewWidth, viewHeight);
    transform *= viewSpec->protectedTransform()->concatenate();
    return transform;
}

RefPtr<SVGViewElement> SVGSVGElement::findViewAnchor(StringView fragmentIdentifier) const
{
    return dynamicDowncast<SVGViewElement>(protectedDocument()->findAnchor(fragmentIdentifier));
}

SVGSVGElement* SVGSVGElement::findRootAnchor(const SVGViewElement* viewElement) const
{
    return dynamicDowncast<SVGSVGElement>(SVGLocatable::nearestViewportElement(viewElement));
}

SVGSVGElement* SVGSVGElement::findRootAnchor(StringView fragmentIdentifier) const
{
    if (RefPtr viewElement = findViewAnchor(fragmentIdentifier))
        return findRootAnchor(viewElement.get());
    return nullptr;
}

bool SVGSVGElement::scrollToFragment(StringView fragmentIdentifier)
{
    CheckedPtr renderer = downcast<RenderLayerModelObject>(this->renderer());

    RefPtr view = m_viewSpec;
    if (view)
        view->reset();

    bool hadUseCurrentView = m_useCurrentView;
    m_useCurrentView = false;

    auto invalidateView = [&](RenderElement& renderer) {
        if (renderer.document().settings().layerBasedSVGEngineEnabled()) {
            renderer.repaint();
            return;
        }

        LegacyRenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
    };

    if (fragmentIdentifier.startsWith("xpointer("_s)) {
        // FIXME: XPointer references are ignored (https://bugs.webkit.org/show_bug.cgi?id=17491)
        if (renderer && hadUseCurrentView)
            invalidateView(*renderer);
        return false;
    }

    if (fragmentIdentifier.startsWith("svgView("_s)) {
        if (!view)
            view = &currentView(); // Create the SVGViewSpec.
        if (view->parseViewSpec(fragmentIdentifier))
            m_useCurrentView = true;
        else
            view->reset();
        if (renderer && (hadUseCurrentView || m_useCurrentView))
            invalidateView(*renderer);
        return m_useCurrentView;
    }

    // Spec: If the SVG fragment identifier addresses a "view" element within an SVG document (e.g., MyDrawing.svg#MyView
    // or MyDrawing.svg#xpointer(id('MyView'))) then the closest ancestor "svg" element is displayed in the viewport.
    // Any view specification attributes included on the given "view" element override the corresponding view specification
    // attributes on the closest ancestor "svg" element.
    if (RefPtr viewElement = findViewAnchor(fragmentIdentifier)) {
        if (RefPtr rootElement = findRootAnchor(viewElement.get())) {
            if (rootElement->m_currentViewElement) {
                ASSERT(rootElement->m_currentViewElement->targetElement() == rootElement);

                // If the viewElement has changed, remove the link from the SVGViewElement to the previously selected SVGSVGElement.
                if (rootElement->m_currentViewElement != viewElement)
                    RefPtr { rootElement->m_currentViewElement }->resetTargetElement();
            }

            if (rootElement->m_currentViewElement != viewElement) {
                rootElement->m_currentViewElement = viewElement;
                RefPtr { rootElement->m_currentViewElement }->setTargetElement(*rootElement);
            }

            rootElement->inheritViewAttributes(*viewElement);
            if (CheckedPtr renderer = rootElement->renderer())
                invalidateView(*renderer);
            m_currentViewFragmentIdentifier = fragmentIdentifier.toString();
            return true;
        }
    }

    // FIXME: We need to decide which <svg> to focus on, and zoom to it.
    // FIXME: We need to actually "highlight" the viewTarget(s).
    return false;
}

Ref<SMILTimeContainer> SVGSVGElement::protectedTimeContainer() const
{
    return m_timeContainer;
}

void SVGSVGElement::resetScrollAnchor()
{
    if (!m_useCurrentView && m_currentViewFragmentIdentifier.isEmpty())
        return;

    if (RefPtr viewSpec = m_viewSpec)
        viewSpec->reset();

    if (!m_currentViewFragmentIdentifier.isEmpty()) {
        if (RefPtr rootElement = findRootAnchor(m_currentViewFragmentIdentifier)) {
            Ref view = rootElement->currentView();
            view->setViewBox(viewBox());
            view->setPreserveAspectRatio(preserveAspectRatio());
            view->setZoomAndPan(zoomAndPan());
            m_currentViewFragmentIdentifier = { };
        }
    }

    m_useCurrentView = false;

    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return;

    if (document().settings().layerBasedSVGEngineEnabled()) {
        renderer->repaint();
        return;
    }

    LegacyRenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
}

void SVGSVGElement::inheritViewAttributes(const SVGViewElement& viewElement)
{
    Ref view = currentView();
    m_useCurrentView = true;

    if (viewElement.hasAttribute(SVGNames::viewBoxAttr))
        view->setViewBox(viewElement.viewBox());
    else
        view->setViewBox(viewBox());

    if (viewElement.hasAttribute(SVGNames::preserveAspectRatioAttr))
        view->setPreserveAspectRatio(viewElement.preserveAspectRatio());
    else
        view->setPreserveAspectRatio(preserveAspectRatio());

    if (viewElement.hasAttribute(SVGNames::zoomAndPanAttr))
        view->setZoomAndPan(viewElement.zoomAndPan());
    else
        view->setZoomAndPan(zoomAndPan());
}

void SVGSVGElement::prepareForDocumentSuspension()
{
    pauseAnimations();
}

void SVGSVGElement::resumeFromDocumentSuspension()
{
    unpauseAnimations();
}

// getElementById on SVGSVGElement is restricted to only the child subtree defined by the <svg> element.
// See http://www.w3.org/TR/SVG11/struct.html#InterfaceSVGSVGElement
Element* SVGSVGElement::getElementById(const AtomString& id)
{
    if (id.isNull())
        return nullptr;

    if (UNLIKELY(!isInTreeScope())) {
        for (auto& element : descendantsOfType<Element>(*this)) {
            if (element.getIdAttribute() == id)
                return &element;
        }
        return nullptr;
    }

    RefPtr element = treeScope().getElementById(id);
    if (element && element->isDescendantOf(*this))
        return element.get();
    if (treeScope().containsMultipleElementsWithId(id)) {
        for (auto& element : *treeScope().getAllElementsById(id)) {
            if (element->isDescendantOf(*this))
                return element.ptr();
        }
    }

    return nullptr;
}

bool SVGSVGElement::isValid() const
{
    return SVGTests::isValid();
}

}
