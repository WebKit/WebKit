/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2015 Apple Inc. All rights reserved.
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
#include "ElementIterator.h"
#include "FrameSelection.h"
#include "MainFrame.h"
#include "RenderSVGResource.h"
#include "RenderSVGRoot.h"
#include "RenderSVGViewportContainer.h"
#include "RenderView.h"
#include "SMILTimeContainer.h"
#include "SVGViewElement.h"
#include "SVGViewSpec.h"
#include "StaticNodeList.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGSVGElement, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH(SVGSVGElement, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_LENGTH(SVGSVGElement, SVGNames::widthAttr, Width, width)
DEFINE_ANIMATED_LENGTH(SVGSVGElement, SVGNames::heightAttr, Height, height)
DEFINE_ANIMATED_BOOLEAN(SVGSVGElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)
DEFINE_ANIMATED_PRESERVEASPECTRATIO(SVGSVGElement, SVGNames::preserveAspectRatioAttr, PreserveAspectRatio, preserveAspectRatio)
DEFINE_ANIMATED_RECT(SVGSVGElement, SVGNames::viewBoxAttr, ViewBox, viewBox)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGSVGElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(x)
    REGISTER_LOCAL_ANIMATED_PROPERTY(y)
    REGISTER_LOCAL_ANIMATED_PROPERTY(width)
    REGISTER_LOCAL_ANIMATED_PROPERTY(height)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_LOCAL_ANIMATED_PROPERTY(viewBox)
    REGISTER_LOCAL_ANIMATED_PROPERTY(preserveAspectRatio)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGSVGElement::SVGSVGElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
    , m_x(LengthModeWidth)
    , m_y(LengthModeHeight)
    , m_width(LengthModeWidth, ASCIILiteral("100%"))
    , m_height(LengthModeHeight, ASCIILiteral("100%"))
    , m_timeContainer(RefPtr<SMILTimeContainer>(SMILTimeContainer::create(this)).releaseNonNull())
{
    ASSERT(hasTagName(SVGNames::svgTag));
    registerAnimatedPropertiesForSVGSVGElement();
    document.registerForDocumentSuspensionCallbacks(this);
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
    document().unregisterForDocumentSuspensionCallbacks(this);
    document().accessSVGExtensions().removeTimeContainer(this);
}

void SVGSVGElement::didMoveToNewDocument(Document* oldDocument)
{
    if (oldDocument)
        oldDocument->unregisterForDocumentSuspensionCallbacks(this);
    document().registerForDocumentSuspensionCallbacks(this);
    SVGGraphicsElement::didMoveToNewDocument(oldDocument);
}

const AtomicString& SVGSVGElement::contentScriptType() const
{
    static NeverDestroyed<AtomicString> defaultScriptType { "text/ecmascript" };
    const AtomicString& type = fastGetAttribute(SVGNames::contentScriptTypeAttr);
    return type.isNull() ? defaultScriptType.get() : type;
}

void SVGSVGElement::setContentScriptType(const AtomicString& type)
{
    setAttribute(SVGNames::contentScriptTypeAttr, type);
}

const AtomicString& SVGSVGElement::contentStyleType() const
{
    static NeverDestroyed<AtomicString> defaultStyleType { "text/css" };
    const AtomicString& type = fastGetAttribute(SVGNames::contentStyleTypeAttr);
    return type.isNull() ? defaultStyleType.get() : type;
}

void SVGSVGElement::setContentStyleType(const AtomicString& type)
{
    setAttribute(SVGNames::contentStyleTypeAttr, type);
}

FloatRect SVGSVGElement::viewport() const
{
    // FIXME: Not implemented.
    return { };
}

float SVGSVGElement::pixelUnitToMillimeterX() const
{
    // There are 25.4 millimeters in an inch.
    return 25.4f / cssPixelsPerInch;
}

float SVGSVGElement::pixelUnitToMillimeterY() const
{
    // There are 25.4 millimeters in an inch.
    return 25.4f / cssPixelsPerInch;
}

float SVGSVGElement::screenPixelToMillimeterX() const
{
    return pixelUnitToMillimeterX();
}

float SVGSVGElement::screenPixelToMillimeterY() const
{
    return pixelUnitToMillimeterY();
}

SVGViewSpec& SVGSVGElement::currentView()
{
    if (!m_viewSpec)
        m_viewSpec = SVGViewSpec::create(this);
    return *m_viewSpec;
}

Frame* SVGSVGElement::frameForCurrentScale() const
{
    // The behavior of currentScale() is undefined when we're dealing with non-standalone SVG documents.
    // If the document is embedded, the scaling is handled by the host renderer.
    if (!inDocument() || !isOutermostSVGSVGElement())
        return nullptr;
    Frame* frame = document().frame();
    return frame && frame->isMainFrame() ? frame : nullptr;
}

float SVGSVGElement::currentScale() const
{
    // When asking from inside an embedded SVG document, a scale value of 1 seems reasonable, as it doesn't
    // know anything about the parent scale.
    Frame* frame = frameForCurrentScale();
    return frame ? frame->pageZoomFactor() : 1;
}

void SVGSVGElement::setCurrentScale(float scale)
{
    if (Frame* frame = frameForCurrentScale())
        frame->setPageZoomFactor(scale);
}

void SVGSVGElement::setCurrentTranslate(const FloatPoint& translation)
{
    if (m_currentTranslate == translation)
        return;
    m_currentTranslate = translation;
    updateCurrentTranslate();
}

void SVGSVGElement::updateCurrentTranslate()
{
    if (RenderObject* object = renderer())
        object->setNeedsLayout();
    if (parentNode() == &document() && document().renderView())
        document().renderView()->repaint();
}

void SVGSVGElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!nearestViewportElement()) {
        // For these events, the outermost <svg> element works like a <body> element does,
        // setting certain event handlers directly on the window object.
        if (name == HTMLNames::onunloadAttr) {
            document().setWindowAttributeEventListener(eventNames().unloadEvent, name, value);
            return;
        }
        if (name == HTMLNames::onresizeAttr) {
            document().setWindowAttributeEventListener(eventNames().resizeEvent, name, value);
            return;
        }
        if (name == HTMLNames::onscrollAttr) {
            document().setWindowAttributeEventListener(eventNames().scrollEvent, name, value);
            return;
        }
        if (name == SVGNames::onzoomAttr) {
            document().setWindowAttributeEventListener(eventNames().zoomEvent, name, value);
            return;
        }
    }

    // For these events, any <svg> element works like a <body> element does,
    // setting certain event handlers directly on the window object.
    // FIXME: Why different from the events above that work only on the outermost <svg> element?
    if (name == HTMLNames::onabortAttr) {
        document().setWindowAttributeEventListener(eventNames().abortEvent, name, value);
        return;
    }
    if (name == HTMLNames::onerrorAttr) {
        document().setWindowAttributeEventListener(eventNames().errorEvent, name, value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        setXBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::yAttr)
        setYBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::widthAttr) {
        SVGLength length = SVGLength::construct(LengthModeWidth, value, parseError, ForbidNegativeLengths);
        if (parseError != NoError || value.isEmpty()) {
            // FIXME: This is definitely the correct behavior for a missing/removed attribute.
            // Not sure it's correct for the empty string or for something that can't be parsed.
            length = SVGLength(LengthModeWidth, ASCIILiteral("100%"));
        }
        setWidthBaseValue(length);
    } else if (name == SVGNames::heightAttr) {
        SVGLength length = SVGLength::construct(LengthModeHeight, value, parseError, ForbidNegativeLengths);
        if (parseError != NoError || value.isEmpty()) {
            // FIXME: This is definitely the correct behavior for a removed attribute.
            // Not sure it's correct for the empty string or for something that can't be parsed.
            length = SVGLength(LengthModeHeight, ASCIILiteral("100%"));
        }
        setHeightBaseValue(length);
    }

    reportAttributeParsingError(parseError, name, value);

    SVGExternalResourcesRequired::parseAttribute(name, value);
    SVGFitToViewBox::parseAttribute(this, name, value);
    SVGZoomAndPan::parseAttribute(*this, name, value);
    SVGGraphicsElement::parseAttribute(name, value);
}

void SVGSVGElement::svgAttributeChanged(const QualifiedName& attrName)
{
    bool updateRelativeLengthsOrViewBox = false;
    if (attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr || attrName == SVGNames::xAttr || attrName == SVGNames::yAttr) {
        invalidateSVGPresentationAttributeStyle();
        updateRelativeLengthsOrViewBox = true;
    }

    if (SVGFitToViewBox::isKnownAttribute(attrName)) {
        updateRelativeLengthsOrViewBox = true; 
        if (auto* renderer = this->renderer())
            renderer->setNeedsTransformUpdate();
    }

    InstanceInvalidationGuard guard(*this);

    if (updateRelativeLengthsOrViewBox
        || SVGLangSpace::isKnownAttribute(attrName)
        || SVGExternalResourcesRequired::isKnownAttribute(attrName)
        || SVGZoomAndPan::isKnownAttribute(attrName)) {
        if (auto renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

unsigned SVGSVGElement::suspendRedraw(unsigned)
{
    return 0;
}

void SVGSVGElement::unsuspendRedraw(unsigned)
{
}

void SVGSVGElement::unsuspendRedrawAll()
{
}

void SVGSVGElement::forceRedraw()
{
}

Ref<NodeList> SVGSVGElement::collectIntersectionOrEnclosureList(const FloatRect& rect, SVGElement* referenceElement, bool (*checkFunction)(const SVGElement*, const FloatRect&))
{
    Vector<Ref<Element>> elements;
    for (auto& element : descendantsOfType<SVGElement>(referenceElement ? *referenceElement : *this)) {
        if (checkFunction(&element, rect))
            elements.append(element);
    }
    return RefPtr<NodeList>(StaticElementList::adopt(elements)).releaseNonNull();
}

Ref<NodeList> SVGSVGElement::getIntersectionList(const FloatRect& rect, SVGElement* referenceElement)
{
    return collectIntersectionOrEnclosureList(rect, referenceElement, checkIntersection);
}

Ref<NodeList> SVGSVGElement::getEnclosureList(const FloatRect& rect, SVGElement* referenceElement)
{
    return collectIntersectionOrEnclosureList(rect, referenceElement, checkEnclosure);
}

bool SVGSVGElement::checkIntersection(const SVGElement* element, const FloatRect& rect)
{
    return element && RenderSVGModelObject::checkIntersection(element->renderer(), rect);
}

bool SVGSVGElement::checkEnclosure(const SVGElement* element, const FloatRect& rect)
{
    return element && RenderSVGModelObject::checkEnclosure(element->renderer(), rect);
}

void SVGSVGElement::deselectAll()
{
    if (Frame* frame = document().frame())
        frame->selection().clear();
}

SVGLength SVGSVGElement::createSVGLength()
{
    return { };
}

SVGAngle SVGSVGElement::createSVGAngle()
{
    return { };
}

SVGPoint SVGSVGElement::createSVGPoint()
{
    return { };
}

SVGMatrix SVGSVGElement::createSVGMatrix()
{
    return { };
}

FloatRect SVGSVGElement::createSVGRect()
{
    return { };
}

SVGTransform SVGSVGElement::createSVGTransform()
{
    return SVGTransform::SVG_TRANSFORM_MATRIX;
}

SVGTransform SVGSVGElement::createSVGTransformFromMatrix(const SVGMatrix& matrix)
{
    return SVGTransform { matrix };
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
            location.scale(zoomFactor, zoomFactor);

            // Be careful here! localToBorderBoxTransform() included the x/y offset coming from the viewBoxToViewTransform(),
            // so we have to subtract it here (original cause of bug #27183)
            transform.translate(location.x() - viewBoxTransform.e(), location.y() - viewBoxTransform.f());

            // Respect scroll offset.
            if (FrameView* view = document().view()) {
                LayoutPoint scrollPosition = view->scrollPosition();
                scrollPosition.scale(zoomFactor, zoomFactor);
                transform.translate(-scrollPosition.x(), -scrollPosition.y());
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

RenderPtr<RenderElement> SVGSVGElement::createElementRenderer(Ref<RenderStyle>&& style, const RenderTreePosition&)
{
    if (isOutermostSVGSVGElement())
        return createRenderer<RenderSVGRoot>(*this, WTF::move(style));
    return createRenderer<RenderSVGViewportContainer>(*this, WTF::move(style));
}

Node::InsertionNotificationRequest SVGSVGElement::insertedInto(ContainerNode& rootParent)
{
    if (rootParent.inDocument()) {
        document().accessSVGExtensions().addTimeContainer(this);

        // Animations are started at the end of document parsing and after firing the load event,
        // but if we miss that train (deferred programmatic element insertion for example) we need
        // to initialize the time container here.
        if (!document().parsing() && !document().processingLoadEvent() && document().loadEventFinished() && !m_timeContainer->isStarted())
            m_timeContainer->begin();
    }
    return SVGGraphicsElement::insertedInto(rootParent);
}

void SVGSVGElement::removedFrom(ContainerNode& rootParent)
{
    if (rootParent.inDocument())
        document().accessSVGExtensions().removeTimeContainer(this);
    SVGGraphicsElement::removedFrom(rootParent);
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
    return width().unitType() != LengthTypePercentage;
}

bool SVGSVGElement::hasIntrinsicHeight() const
{
    return height().unitType() != LengthTypePercentage;
}

Length SVGSVGElement::intrinsicWidth() const
{
    if (width().unitType() == LengthTypePercentage)
        return Length(0, Fixed);

    SVGLengthContext lengthContext(this);
    return Length(width().value(lengthContext), Fixed);
}

Length SVGSVGElement::intrinsicHeight() const
{
    if (height().unitType() == LengthTypePercentage)
        return Length(0, Fixed);

    SVGLengthContext lengthContext(this);
    return Length(height().value(lengthContext), Fixed);
}

AffineTransform SVGSVGElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    if (!m_useCurrentView || !m_viewSpec)
        return SVGFitToViewBox::viewBoxToViewTransform(currentViewBoxRect(), preserveAspectRatio(), viewWidth, viewHeight);

    AffineTransform transform = SVGFitToViewBox::viewBoxToViewTransform(currentViewBoxRect(), m_viewSpec->preserveAspectRatio(), viewWidth, viewHeight);
    m_viewSpec->transformBaseValue().concatenate(transform);
    return transform;
}

void SVGSVGElement::scrollToAnchor(const String& fragmentIdentifier, Element* anchorNode)
{
    auto renderer = this->renderer();
    SVGViewSpec* view = m_viewSpec.get();
    if (view)
        view->reset();

    bool hadUseCurrentView = m_useCurrentView;
    m_useCurrentView = false;

    if (fragmentIdentifier.startsWith("xpointer(")) {
        // FIXME: XPointer references are ignored (https://bugs.webkit.org/show_bug.cgi?id=17491)
        if (renderer && hadUseCurrentView)
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
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
        return;
    }

    // Spec: If the SVG fragment identifier addresses a "view" element within an SVG document (e.g., MyDrawing.svg#MyView
    // or MyDrawing.svg#xpointer(id('MyView'))) then the closest ancestor "svg" element is displayed in the viewport.
    // Any view specification attributes included on the given "view" element override the corresponding view specification
    // attributes on the closest ancestor "svg" element.
    if (is<SVGViewElement>(anchorNode)) {
        auto& viewElement = downcast<SVGViewElement>(*anchorNode);
        auto* viewportElement = SVGLocatable::nearestViewportElement(&viewElement);
        if (is<SVGSVGElement>(viewportElement)) {
            auto& element = downcast<SVGSVGElement>(*viewportElement);
            element.inheritViewAttributes(viewElement);
            if (auto* renderer = element.renderer())
                RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    // FIXME: We need to decide which <svg> to focus on, and zoom to it.
    // FIXME: We need to actually "highlight" the viewTarget(s).
}

void SVGSVGElement::inheritViewAttributes(const SVGViewElement& viewElement)
{
    SVGViewSpec& view = currentView();
    m_useCurrentView = true;

    if (viewElement.hasAttribute(SVGNames::viewBoxAttr))
        view.setViewBoxBaseValue(viewElement.viewBox());
    else
        view.setViewBoxBaseValue(viewBox());

    if (viewElement.hasAttribute(SVGNames::preserveAspectRatioAttr))
        view.setPreserveAspectRatioBaseValue(viewElement.preserveAspectRatioBaseValue());
    else
        view.setPreserveAspectRatioBaseValue(preserveAspectRatioBaseValue());

    if (viewElement.hasAttribute(SVGNames::zoomAndPanAttr))
        view.setZoomAndPanBaseValue(viewElement.zoomAndPan());
    else
        view.setZoomAndPanBaseValue(zoomAndPan());
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
Element* SVGSVGElement::getElementById(const AtomicString& id)
{
    Element* element = treeScope().getElementById(id);
    if (element && element->isDescendantOf(this))
        return element;
    if (treeScope().containsMultipleElementsWithId(id)) {
        for (auto* element : *treeScope().getAllElementsById(id)) {
            if (element->isDescendantOf(this))
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
