/*
 * Copyright (C) 2004, 2005, 2006, 2019 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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
#include "Frame.h"
#include "FrameSelection.h"
#include "RenderSVGResource.h"
#include "RenderSVGRoot.h"
#include "RenderSVGViewportContainer.h"
#include "RenderView.h"
#include "SMILTimeContainer.h"
#include "SVGAngle.h"
#include "SVGDocumentExtensions.h"
#include "SVGLength.h"
#include "SVGMatrix.h"
#include "SVGNumber.h"
#include "SVGPoint.h"
#include "SVGRect.h"
#include "SVGTransform.h"
#include "SVGViewElement.h"
#include "SVGViewSpec.h"
#include "StaticNodeList.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGSVGElement);

inline SVGSVGElement::SVGSVGElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
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
    if (m_viewSpec)
        m_viewSpec->resetContextElement();
    document().unregisterForDocumentSuspensionCallbacks(*this);
    document().accessSVGExtensions().removeTimeContainer(*this);
}

void SVGSVGElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    oldDocument.unregisterForDocumentSuspensionCallbacks(*this);
    document().registerForDocumentSuspensionCallbacks(*this);
    SVGGraphicsElement::didMoveToNewDocument(oldDocument, newDocument);
}

SVGViewSpec& SVGSVGElement::currentView()
{
    if (!m_viewSpec)
        m_viewSpec = SVGViewSpec::create(*this);
    return *m_viewSpec;
}

RefPtr<Frame> SVGSVGElement::frameForCurrentScale() const
{
    // The behavior of currentScale() is undefined when we're dealing with non-standalone SVG documents.
    // If the document is embedded, the scaling is handled by the host renderer.
    if (!isConnected() || !isOutermostSVGSVGElement())
        return nullptr;
    auto frame = makeRefPtr(document().frame());
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
    if (RenderObject* object = renderer())
        object->setNeedsLayout();
    if (parentNode() == &document() && document().renderView())
        document().renderView()->repaint();
}

void SVGSVGElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (!nearestViewportElement() && isConnected()) {
        // For these events, the outermost <svg> element works like a <body> element does,
        // setting certain event handlers directly on the window object.
        if (name == HTMLNames::onunloadAttr) {
            document().setWindowAttributeEventListener(eventNames().unloadEvent, name, value, mainThreadNormalWorld());
            return;
        }
        if (name == HTMLNames::onresizeAttr) {
            document().setWindowAttributeEventListener(eventNames().resizeEvent, name, value, mainThreadNormalWorld());
            return;
        }
        if (name == HTMLNames::onscrollAttr) {
            document().setWindowAttributeEventListener(eventNames().scrollEvent, name, value, mainThreadNormalWorld());
            return;
        }
        if (name == SVGNames::onzoomAttr) {
            document().setWindowAttributeEventListener(eventNames().zoomEvent, name, value, mainThreadNormalWorld());
            return;
        }
        if (name == HTMLNames::onabortAttr) {
            document().setWindowAttributeEventListener(eventNames().abortEvent, name, value, mainThreadNormalWorld());
            return;
        }
        if (name == HTMLNames::onerrorAttr) {
            document().setWindowAttributeEventListener(eventNames().errorEvent, name, value, mainThreadNormalWorld());
            return;
        }
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        m_x->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::yAttr)
        m_y->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));
    else if (name == SVGNames::widthAttr) {
        auto length = SVGLengthValue::construct(SVGLengthMode::Width, value, parseError, SVGLengthNegativeValuesMode::Forbid);
        if (parseError != NoError || value.isEmpty()) {
            // FIXME: This is definitely the correct behavior for a missing/removed attribute.
            // Not sure it's correct for the empty string or for something that can't be parsed.
            length = SVGLengthValue(SVGLengthMode::Width, "100%"_s);
        }
        m_width->setBaseValInternal(length);
    } else if (name == SVGNames::heightAttr) {
        auto length = SVGLengthValue::construct(SVGLengthMode::Height, value, parseError, SVGLengthNegativeValuesMode::Forbid);
        if (parseError != NoError || value.isEmpty()) {
            // FIXME: This is definitely the correct behavior for a removed attribute.
            // Not sure it's correct for the empty string or for something that can't be parsed.
            length = SVGLengthValue(SVGLengthMode::Height, "100%"_s);
        }
        m_height->setBaseValInternal(length);
    }

    reportAttributeParsingError(parseError, name, value);

    SVGGraphicsElement::parseAttribute(name, value);
    SVGFitToViewBox::parseAttribute(name, value);
    SVGZoomAndPan::parseAttribute(name, value);
}

void SVGSVGElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        invalidateSVGPresentationAttributeStyle();

        if (auto renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }
    
    if (SVGFitToViewBox::isKnownAttribute(attrName)) {
        if (auto* renderer = this->renderer()) {
            renderer->setNeedsTransformUpdate();
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

Ref<NodeList> SVGSVGElement::collectIntersectionOrEnclosureList(SVGRect& rect, SVGElement* referenceElement, bool (*checkFunction)(SVGElement&, SVGRect&))
{
    Vector<Ref<Element>> elements;
    for (auto& element : descendantsOfType<SVGElement>(referenceElement ? *referenceElement : *this)) {
        if (checkFunction(element, rect))
            elements.append(element);
    }
    return StaticElementList::create(WTFMove(elements));
}

static bool checkIntersectionWithoutUpdatingLayout(SVGElement& element, SVGRect& rect)
{
    return RenderSVGModelObject::checkIntersection(element.renderer(), rect.value());
}
    
static bool checkEnclosureWithoutUpdatingLayout(SVGElement& element, SVGRect& rect)
{
    return RenderSVGModelObject::checkEnclosure(element.renderer(), rect.value());
}

Ref<NodeList> SVGSVGElement::getIntersectionList(SVGRect& rect, SVGElement* referenceElement)
{
    document().updateLayoutIgnorePendingStylesheets();
    return collectIntersectionOrEnclosureList(rect, referenceElement, checkIntersectionWithoutUpdatingLayout);
}

Ref<NodeList> SVGSVGElement::getEnclosureList(SVGRect& rect, SVGElement* referenceElement)
{
    document().updateLayoutIgnorePendingStylesheets();
    return collectIntersectionOrEnclosureList(rect, referenceElement, checkEnclosureWithoutUpdatingLayout);
}

bool SVGSVGElement::checkIntersection(Ref<SVGElement>&& element, SVGRect& rect)
{
    element->document().updateLayoutIgnorePendingStylesheets();
    return checkIntersectionWithoutUpdatingLayout(element, rect);
}

bool SVGSVGElement::checkEnclosure(Ref<SVGElement>&& element, SVGRect& rect)
{
    element->document().updateLayoutIgnorePendingStylesheets();
    return checkEnclosureWithoutUpdatingLayout(element, rect);
}

void SVGSVGElement::deselectAll()
{
    if (auto frame = makeRefPtr(document().frame()))
        frame->selection().clear();
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
    if (matrixInit.a.hasValue())
        transform.setA(matrixInit.a.value());
    if (matrixInit.b.hasValue())
        transform.setB(matrixInit.b.value());
    if (matrixInit.c.hasValue())
        transform.setC(matrixInit.c.value());
    if (matrixInit.d.hasValue())
        transform.setD(matrixInit.d.value());
    if (matrixInit.e.hasValue())
        transform.setE(matrixInit.e.value());
    if (matrixInit.f.hasValue())
        transform.setF(matrixInit.f.value());
    return SVGTransform::create(transform);
}

AffineTransform SVGSVGElement::localCoordinateSpaceTransform(SVGLocatable::CTMScope mode) const
{
    AffineTransform viewBoxTransform;
    if (!hasEmptyViewBox()) {
        FloatSize size = currentViewportSize();
        viewBoxTransform = viewBoxToViewTransform(size.width(), size.height());
    }

    AffineTransform transform;
    if (!isOutermostSVGSVGElement()) {
        SVGLengthContext lengthContext(this);
        transform.translate(x().value(lengthContext), y().value(lengthContext));
    } else if (mode == SVGLocatable::ScreenScope) {
        if (auto* renderer = this->renderer()) {
            FloatPoint location;
            float zoomFactor = 1;

            // At the SVG/HTML boundary (aka RenderSVGRoot), we apply the localToBorderBoxTransform 
            // to map an element from SVG viewport coordinates to CSS box coordinates.
            // RenderSVGRoot's localToAbsolute method expects CSS box coordinates.
            // We also need to adjust for the zoom level factored into CSS coordinates (bug #96361).
            if (is<RenderSVGRoot>(*renderer)) {
                location = downcast<RenderSVGRoot>(*renderer).localToBorderBoxTransform().mapPoint(location);
                zoomFactor = 1 / renderer->style().effectiveZoom();
            }

            // Translate in our CSS parent coordinate space
            // FIXME: This doesn't work correctly with CSS transforms.
            location = renderer->localToAbsolute(location, UseTransforms);
            location.scale(zoomFactor);

            // Be careful here! localToBorderBoxTransform() included the x/y offset coming from the viewBoxToViewTransform(),
            // so we have to subtract it here (original cause of bug #27183)
            transform.translate(location.x() - viewBoxTransform.e(), location.y() - viewBoxTransform.f());

            // Respect scroll offset.
            if (auto view = makeRefPtr(document().view())) {
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
    // but many things in FrameView and SVGImage depend on the RenderSVGRoot when
    // they should instead depend on the RenderView.
    // https://bugs.webkit.org/show_bug.cgi?id=103493
    if (document().documentElement() == this)
        return true;
    return StyledElement::rendererIsNeeded(style);
}

RenderPtr<RenderElement> SVGSVGElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (isOutermostSVGSVGElement())
        return createRenderer<RenderSVGRoot>(*this, WTFMove(style));
    return createRenderer<RenderSVGViewportContainer>(*this, WTFMove(style));
}

Node::InsertedIntoAncestorResult SVGSVGElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    if (insertionType.connectedToDocument) {
        document().accessSVGExtensions().addTimeContainer(*this);
        if (!document().accessSVGExtensions().areAnimationsPaused())
            unpauseAnimations();

        // Animations are started at the end of document parsing and after firing the load event,
        // but if we miss that train (deferred programmatic element insertion for example) we need
        // to initialize the time container here.
        if (!document().parsing() && !document().processingLoadEvent() && document().loadEventFinished() && !m_timeContainer->isStarted())
            m_timeContainer->begin();
    }
    return SVGGraphicsElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
}

void SVGSVGElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.disconnectedFromDocument) {
        document().accessSVGExtensions().removeTimeContainer(*this);
        pauseAnimations();
    }
    SVGGraphicsElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

void SVGSVGElement::pauseAnimations()
{
    if (!m_timeContainer->isPaused())
        m_timeContainer->pause();
}

void SVGSVGElement::unpauseAnimations()
{
    if (m_timeContainer->isPaused())
        m_timeContainer->resume();
}

bool SVGSVGElement::animationsPaused() const
{
    return m_timeContainer->isPaused();
}

bool SVGSVGElement::hasActiveAnimation() const
{
    return m_timeContainer->isActive();
}

float SVGSVGElement::getCurrentTime() const
{
    return narrowPrecisionToFloat(m_timeContainer->elapsed().value());
}

void SVGSVGElement::setCurrentTime(float seconds)
{
    if (!std::isfinite(seconds))
        return;
    m_timeContainer->setElapsed(std::max(seconds, 0.0f));
}

bool SVGSVGElement::selfHasRelativeLengths() const
{
    return x().isRelative()
        || y().isRelative()
        || width().isRelative()
        || height().isRelative()
        || hasAttribute(SVGNames::viewBoxAttr);
}

FloatRect SVGSVGElement::currentViewBoxRect() const
{
    if (m_useCurrentView)
        return m_viewSpec ? m_viewSpec->viewBox() : FloatRect();

    FloatRect viewBox = this->viewBox();
    if (!viewBox.isEmpty())
        return viewBox;

    if (!is<RenderSVGRoot>(renderer()))
        return { };
    if (!downcast<RenderSVGRoot>(*renderer()).isEmbeddedThroughSVGImage())
        return { };

    Length intrinsicWidth = this->intrinsicWidth();
    Length intrinsicHeight = this->intrinsicHeight();
    if (!intrinsicWidth.isFixed() || !intrinsicHeight.isFixed())
        return { };

    // If no viewBox is specified but non-relative width/height values, then we
    // should always synthesize a viewBox if we're embedded through a SVGImage.    
    return { 0, 0, floatValueForLength(intrinsicWidth, 0), floatValueForLength(intrinsicHeight, 0) };
}

FloatSize SVGSVGElement::currentViewportSize() const
{
    FloatSize viewportSize;

    if (renderer()) {
        if (is<RenderSVGRoot>(*renderer())) {
            auto& root = downcast<RenderSVGRoot>(*renderer());
            viewportSize = root.contentBoxRect().size() / root.style().effectiveZoom();
        } else
            viewportSize = downcast<RenderSVGViewportContainer>(*renderer()).viewport().size();
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
        return Length(0, Fixed);

    SVGLengthContext lengthContext(this);
    return Length(width().value(lengthContext), Fixed);
}

Length SVGSVGElement::intrinsicHeight() const
{
    if (height().lengthType() == SVGLengthType::Percentage)
        return Length(0, Fixed);

    SVGLengthContext lengthContext(this);
    return Length(height().value(lengthContext), Fixed);
}

AffineTransform SVGSVGElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    if (!m_useCurrentView || !m_viewSpec)
        return SVGFitToViewBox::viewBoxToViewTransform(currentViewBoxRect(), preserveAspectRatio(), viewWidth, viewHeight);

    AffineTransform transform = SVGFitToViewBox::viewBoxToViewTransform(currentViewBoxRect(), m_viewSpec->preserveAspectRatio(), viewWidth, viewHeight);
    transform *= m_viewSpec->transform()->concatenate();
    return transform;
}

SVGViewElement* SVGSVGElement::findViewAnchor(const String& fragmentIdentifier) const
{
    auto* anchorElement = document().findAnchor(fragmentIdentifier);
    return is<SVGViewElement>(anchorElement) ? downcast<SVGViewElement>(anchorElement): nullptr;
}

SVGSVGElement* SVGSVGElement::findRootAnchor(const SVGViewElement* viewElement) const
{
    auto* viewportElement = SVGLocatable::nearestViewportElement(viewElement);
    return is<SVGSVGElement>(viewportElement) ? downcast<SVGSVGElement>(viewportElement) : nullptr;
}

SVGSVGElement* SVGSVGElement::findRootAnchor(const String& fragmentIdentifier) const
{
    if (auto* viewElement = findViewAnchor(fragmentIdentifier))
        return findRootAnchor(viewElement);
    return nullptr;
}

bool SVGSVGElement::scrollToFragment(const String& fragmentIdentifier)
{
    auto renderer = this->renderer();
    auto view = m_viewSpec;
    if (view)
        view->reset();

    bool hadUseCurrentView = m_useCurrentView;
    m_useCurrentView = false;

    if (fragmentIdentifier.startsWith("xpointer(")) {
        // FIXME: XPointer references are ignored (https://bugs.webkit.org/show_bug.cgi?id=17491)
        if (renderer && hadUseCurrentView)
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return false;
    }

    if (fragmentIdentifier.startsWith("svgView(")) {
        if (!view)
            view = &currentView(); // Create the SVGViewSpec.
        if (view->parseViewSpec(fragmentIdentifier))
            m_useCurrentView = true;
        else
            view->reset();
        if (renderer && (hadUseCurrentView || m_useCurrentView))
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return m_useCurrentView;
    }

    // Spec: If the SVG fragment identifier addresses a "view" element within an SVG document (e.g., MyDrawing.svg#MyView
    // or MyDrawing.svg#xpointer(id('MyView'))) then the closest ancestor "svg" element is displayed in the viewport.
    // Any view specification attributes included on the given "view" element override the corresponding view specification
    // attributes on the closest ancestor "svg" element.
    if (auto* viewElement = findViewAnchor(fragmentIdentifier)) {
        if (auto* rootElement = findRootAnchor(viewElement)) {
            if (rootElement->m_currentViewElement) {
                ASSERT(rootElement->m_currentViewElement->targetElement() == rootElement);

                // If the viewElement has changed, remove the link from the SVGViewElement to the previously selected SVGSVGElement.
                if (rootElement->m_currentViewElement != viewElement)
                    rootElement->m_currentViewElement->resetTargetElement();
            }

            if (rootElement->m_currentViewElement != viewElement) {
                rootElement->m_currentViewElement = viewElement;
                rootElement->m_currentViewElement->setTargetElement(*rootElement);
            }

            rootElement->inheritViewAttributes(*viewElement);
            if (auto* renderer = rootElement->renderer())
                RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
            m_currentViewFragmentIdentifier = fragmentIdentifier;
            return true;
        }
    }

    // FIXME: We need to decide which <svg> to focus on, and zoom to it.
    // FIXME: We need to actually "highlight" the viewTarget(s).
    return false;
}

void SVGSVGElement::resetScrollAnchor()
{
    if (!m_useCurrentView && m_currentViewFragmentIdentifier.isEmpty())
        return;

    if (m_viewSpec)
        m_viewSpec->reset();

    if (!m_currentViewFragmentIdentifier.isEmpty()) {
        if (auto* rootElement = findRootAnchor(m_currentViewFragmentIdentifier)) {
            SVGViewSpec& view = rootElement->currentView();
            view.setViewBox(viewBox());
            view.setPreserveAspectRatio(preserveAspectRatio());
            view.setZoomAndPan(zoomAndPan());
            m_currentViewFragmentIdentifier = { };
        }
    }

    m_useCurrentView = false;
    if (renderer())
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer());
}

void SVGSVGElement::inheritViewAttributes(const SVGViewElement& viewElement)
{
    SVGViewSpec& view = currentView();
    m_useCurrentView = true;

    if (viewElement.hasAttribute(SVGNames::viewBoxAttr))
        view.setViewBox(viewElement.viewBox());
    else
        view.setViewBox(viewBox());

    if (viewElement.hasAttribute(SVGNames::preserveAspectRatioAttr))
        view.setPreserveAspectRatio(viewElement.preserveAspectRatio());
    else
        view.setPreserveAspectRatio(preserveAspectRatio());

    if (viewElement.hasAttribute(SVGNames::zoomAndPanAttr))
        view.setZoomAndPan(viewElement.zoomAndPan());
    else
        view.setZoomAndPan(zoomAndPan());
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

    auto element = makeRefPtr(treeScope().getElementById(id));
    if (element && element->isDescendantOf(*this))
        return element.get();
    if (treeScope().containsMultipleElementsWithId(id)) {
        for (auto* element : *treeScope().getAllElementsById(id)) {
            if (element->isDescendantOf(*this))
                return element;
        }
    }
    return nullptr;
}

bool SVGSVGElement::isValid() const
{
    return SVGTests::isValid();
}

}
