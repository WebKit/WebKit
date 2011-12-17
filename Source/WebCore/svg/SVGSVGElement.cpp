/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGSVGElement.h"

#include "AffineTransform.h"
#include "Attribute.h"
#include "CSSHelper.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "RenderSVGResource.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGRoot.h"
#include "RenderSVGViewportContainer.h"
#include "SMILTimeContainer.h"
#include "SVGAngle.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGViewElement.h"
#include "SVGViewSpec.h"
#include "SVGZoomEvent.h"
#include "ScriptEventListener.h"
#include "StaticNodeList.h"
#include <wtf/StdLibExtras.h>

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
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGStyledLocatableElement)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTests)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGSVGElement::SVGSVGElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledLocatableElement(tagName, doc)
    , m_x(LengthModeWidth)
    , m_y(LengthModeHeight)
    , m_width(LengthModeWidth, "100%")
    , m_height(LengthModeHeight, "100%") 
    , m_useCurrentView(false)
    , m_timeContainer(SMILTimeContainer::create(this))
{
    ASSERT(hasTagName(SVGNames::svgTag));
    registerAnimatedPropertiesForSVGSVGElement();
    doc->registerForPageCacheSuspensionCallbacks(this);
}

PassRefPtr<SVGSVGElement> SVGSVGElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGSVGElement(tagName, document));
}

SVGSVGElement::~SVGSVGElement()
{
    document()->unregisterForPageCacheSuspensionCallbacks(this);
    // There are cases where removedFromDocument() is not called.
    // see ContainerNode::removeAllChildren, called by its destructor.
    document()->accessSVGExtensions()->removeTimeContainer(this);
}

void SVGSVGElement::willMoveToNewOwnerDocument()
{
    document()->unregisterForPageCacheSuspensionCallbacks(this);
    SVGStyledLocatableElement::willMoveToNewOwnerDocument();
}

void SVGSVGElement::didMoveToNewOwnerDocument()
{
    document()->registerForPageCacheSuspensionCallbacks(this);
    SVGStyledLocatableElement::didMoveToNewOwnerDocument();
}

const AtomicString& SVGSVGElement::contentScriptType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, defaultValue, ("text/ecmascript"));
    const AtomicString& n = fastGetAttribute(SVGNames::contentScriptTypeAttr);
    return n.isNull() ? defaultValue : n;
}

void SVGSVGElement::setContentScriptType(const AtomicString& type)
{
    setAttribute(SVGNames::contentScriptTypeAttr, type);
}

const AtomicString& SVGSVGElement::contentStyleType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, defaultValue, ("text/css"));
    const AtomicString& n = fastGetAttribute(SVGNames::contentStyleTypeAttr);
    return n.isNull() ? defaultValue : n;
}

void SVGSVGElement::setContentStyleType(const AtomicString& type)
{
    setAttribute(SVGNames::contentStyleTypeAttr, type);
}

FloatRect SVGSVGElement::viewport() const
{
    // FIXME: This method doesn't follow the spec and is basically untested. Parent documents are not considered here.
    SVGLengthContext lengthContext(this);
    FloatRect viewRectangle;
    if (!isOutermostSVG())
        viewRectangle.setLocation(FloatPoint(x().value(lengthContext), y().value(lengthContext)));

    viewRectangle.setSize(FloatSize(width().value(lengthContext), height().value(lengthContext)));    
    return viewBoxToViewTransform(viewRectangle.width(), viewRectangle.height()).mapRect(viewRectangle);
}

float SVGSVGElement::pixelUnitToMillimeterX() const
{
    // 2.54 / cssPixelsPerInch gives CM.
    return (2.54f / cssPixelsPerInch) * 10.0f;
}

float SVGSVGElement::pixelUnitToMillimeterY() const
{
    // 2.54 / cssPixelsPerInch gives CM.
    return (2.54f / cssPixelsPerInch) * 10.0f;
}

float SVGSVGElement::screenPixelToMillimeterX() const
{
    return pixelUnitToMillimeterX();
}

float SVGSVGElement::screenPixelToMillimeterY() const
{
    return pixelUnitToMillimeterY();
}

bool SVGSVGElement::useCurrentView() const
{
    return m_useCurrentView;
}

void SVGSVGElement::setUseCurrentView(bool currentView)
{
    m_useCurrentView = currentView;
}

SVGViewSpec* SVGSVGElement::currentView() const
{
    if (!m_viewSpec)
        m_viewSpec = adoptPtr(new SVGViewSpec(const_cast<SVGSVGElement*>(this)));
    return m_viewSpec.get();
}

float SVGSVGElement::currentScale() const
{
    if (!inDocument() || !isOutermostSVG())
        return 1;

    Frame* frame = document()->frame();
    if (!frame)
        return 1;

    FrameTree* frameTree = frame->tree();
    ASSERT(frameTree);

    // The behaviour of currentScale() is undefined, when we're dealing with non-standalone SVG documents.
    // If the svg is embedded, the scaling is handled by the host renderer, so when asking from inside
    // the SVG document, a scale value of 1 seems reasonable, as it doesn't know anything about the parent scale.
    return frameTree->parent() ? 1 : frame->pageZoomFactor();
}

void SVGSVGElement::setCurrentScale(float scale)
{
    if (!inDocument() || !isOutermostSVG())
        return;

    Frame* frame = document()->frame();
    if (!frame)
        return;

    FrameTree* frameTree = frame->tree();
    ASSERT(frameTree);

    // The behaviour of setCurrentScale() is undefined, when we're dealing with non-standalone SVG documents.
    // We choose the ignore this call, it's pretty useless to support calling setCurrentScale() from within
    // an embedded SVG document, for the same reasons as in currentScale() - needs resolution by SVG WG.
    if (frameTree->parent())
        return;

    frame->setPageZoomFactor(scale);
}

void SVGSVGElement::setCurrentTranslate(const FloatPoint& translation)
{
    m_translation = translation;
    updateCurrentTranslate();
}

void SVGSVGElement::updateCurrentTranslate()
{
    if (RenderObject* object = renderer())
        object->setNeedsLayout(true);

    if (parentNode() == document() && document()->renderer())
        document()->renderer()->repaint();
}

void SVGSVGElement::parseMappedAttribute(Attribute* attr)
{
    SVGParsingError parseError = NoError;

    if (!nearestViewportElement()) {
        bool setListener = true;

        // Only handle events if we're the outermost <svg> element
        if (attr->name() == HTMLNames::onunloadAttr)
            document()->setWindowAttributeEventListener(eventNames().unloadEvent, createAttributeEventListener(document()->frame(), attr));
        else if (attr->name() == HTMLNames::onresizeAttr)
            document()->setWindowAttributeEventListener(eventNames().resizeEvent, createAttributeEventListener(document()->frame(), attr));
        else if (attr->name() == HTMLNames::onscrollAttr)
            document()->setWindowAttributeEventListener(eventNames().scrollEvent, createAttributeEventListener(document()->frame(), attr));
        else if (attr->name() == SVGNames::onzoomAttr)
            document()->setWindowAttributeEventListener(eventNames().zoomEvent, createAttributeEventListener(document()->frame(), attr));
        else
            setListener = false;
 
        if (setListener)
            return;
    }

    if (attr->name() == HTMLNames::onabortAttr)
        document()->setWindowAttributeEventListener(eventNames().abortEvent, createAttributeEventListener(document()->frame(), attr));
    else if (attr->name() == HTMLNames::onerrorAttr)
        document()->setWindowAttributeEventListener(eventNames().errorEvent, createAttributeEventListener(document()->frame(), attr));
    else if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength::construct(LengthModeWidth, attr->value(), parseError));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength::construct(LengthModeHeight, attr->value(), parseError));
    else if (attr->name() == SVGNames::widthAttr) {
        setWidthBaseValue(SVGLength::construct(LengthModeWidth, attr->value(), parseError, ForbidNegativeLengths));
        addCSSProperty(attr, CSSPropertyWidth, attr->value());
    } else if (attr->name() == SVGNames::heightAttr) {
        setHeightBaseValue(SVGLength::construct(LengthModeHeight, attr->value(), parseError, ForbidNegativeLengths));
        addCSSProperty(attr, CSSPropertyHeight, attr->value());
    } else if (SVGTests::parseMappedAttribute(attr)
               || SVGLangSpace::parseMappedAttribute(attr)
               || SVGExternalResourcesRequired::parseMappedAttribute(attr)
               || SVGFitToViewBox::parseMappedAttribute(document(), attr)
               || SVGZoomAndPan::parseMappedAttribute(attr)) {
    } else
        SVGStyledLocatableElement::parseMappedAttribute(attr);

    reportAttributeParsingError(parseError, attr);
}

// This hack will not handle the case where we're setting a width/height
// on a root <svg> via svg.width.baseValue = when it has none.
static void updateCSSForAttribute(SVGSVGElement* element, const QualifiedName& attrName, CSSPropertyID property, const SVGLength& value)
{
    Attribute* attribute = element->attributes(false)->getAttributeItem(attrName);
    if (!attribute || !attribute->isMappedAttribute())
        return;
    element->addCSSProperty(attribute, property, value.valueAsString());
}

void SVGSVGElement::svgAttributeChanged(const QualifiedName& attrName)
{ 
    // FIXME: Ugly, ugly hack to around that parseMappedAttribute is not called
    // when svg.width.baseValue = 100 is evaluated.
    // Thus the CSS length value for width is not updated, and width() computeLogicalWidth()
    // calculations on RenderSVGRoot will be wrong.
    // https://bugs.webkit.org/show_bug.cgi?id=25387
    bool updateRelativeLengths = false;
    if (attrName == SVGNames::widthAttr) {
        updateCSSForAttribute(this, attrName, CSSPropertyWidth, widthBaseValue());
        updateRelativeLengths = true;
    } else if (attrName == SVGNames::heightAttr) {
        updateCSSForAttribute(this, attrName, CSSPropertyHeight, heightBaseValue());
        updateRelativeLengths = true;
    }

    if (updateRelativeLengths
        || attrName == SVGNames::xAttr
        || attrName == SVGNames::yAttr
        || SVGFitToViewBox::isKnownAttribute(attrName)) {
        updateRelativeLengths = true;
        updateRelativeLengthsInformation();
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);
    if (SVGTests::handleAttributeChange(this, attrName))
        return;

    if (updateRelativeLengths
        || SVGLangSpace::isKnownAttribute(attrName)
        || SVGExternalResourcesRequired::isKnownAttribute(attrName)
        || SVGZoomAndPan::isKnownAttribute(attrName)) {
        if (renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer());
        return;
    }

    SVGStyledElement::svgAttributeChanged(attrName);
}

unsigned SVGSVGElement::suspendRedraw(unsigned /* maxWaitMilliseconds */)
{
    // FIXME: Implement me (see bug 11275)
    return 0;
}

void SVGSVGElement::unsuspendRedraw(unsigned /* suspendHandleId */)
{
    // FIXME: Implement me (see bug 11275)
}

void SVGSVGElement::unsuspendRedrawAll()
{
    // FIXME: Implement me (see bug 11275)
}

void SVGSVGElement::forceRedraw()
{
    // FIXME: Implement me (see bug 11275)
}

PassRefPtr<NodeList> SVGSVGElement::collectIntersectionOrEnclosureList(const FloatRect& rect, SVGElement* referenceElement, CollectIntersectionOrEnclosure collect) const
{
    Vector<RefPtr<Node> > nodes;
    Node* node = traverseNextNode(referenceElement ? referenceElement : this);
    while (node) {
        if (node->isSVGElement()) { 
            if (collect == CollectIntersectionList) {
                if (checkIntersection(static_cast<SVGElement*>(node), rect))
                    nodes.append(node);
            } else {
                if (checkEnclosure(static_cast<SVGElement*>(node), rect))
                    nodes.append(node);
            }
        }

        node = node->traverseNextNode(referenceElement ? referenceElement : this);
    }
    return StaticNodeList::adopt(nodes);
}

PassRefPtr<NodeList> SVGSVGElement::getIntersectionList(const FloatRect& rect, SVGElement* referenceElement) const
{
    return collectIntersectionOrEnclosureList(rect, referenceElement, CollectIntersectionList);
}

PassRefPtr<NodeList> SVGSVGElement::getEnclosureList(const FloatRect& rect, SVGElement* referenceElement) const
{
    return collectIntersectionOrEnclosureList(rect, referenceElement, CollectEnclosureList);
}

bool SVGSVGElement::checkIntersection(SVGElement* element, const FloatRect& rect) const
{
    return RenderSVGModelObject::checkIntersection(element->renderer(), rect);
}

bool SVGSVGElement::checkEnclosure(SVGElement* element, const FloatRect& rect) const
{
    return RenderSVGModelObject::checkEnclosure(element->renderer(), rect);
}

void SVGSVGElement::deselectAll()
{
    if (Frame* frame = document()->frame())
        frame->selection()->clear();
}

float SVGSVGElement::createSVGNumber()
{
    return 0.0f;
}

SVGLength SVGSVGElement::createSVGLength()
{
    return SVGLength();
}

SVGAngle SVGSVGElement::createSVGAngle()
{
    return SVGAngle();
}

FloatPoint SVGSVGElement::createSVGPoint()
{
    return FloatPoint();
}

SVGMatrix SVGSVGElement::createSVGMatrix()
{
    return SVGMatrix();
}

FloatRect SVGSVGElement::createSVGRect()
{
    return FloatRect();
}

SVGTransform SVGSVGElement::createSVGTransform()
{
    return SVGTransform(SVGTransform::SVG_TRANSFORM_MATRIX);
}

SVGTransform SVGSVGElement::createSVGTransformFromMatrix(const SVGMatrix& matrix)
{
    return SVGTransform(static_cast<const AffineTransform&>(matrix));
}

AffineTransform SVGSVGElement::localCoordinateSpaceTransform(SVGLocatable::CTMScope mode) const
{
    // This method resolves length manually, w/o involving the render tree. This is desired, as getCTM()/getScreenCTM()/.. have to work without a renderer.
    SVGLengthContext lengthContext(this);

    AffineTransform viewBoxTransform;
    if (attributes()->getAttributeItem(SVGNames::viewBoxAttr))
        viewBoxTransform = viewBoxToViewTransform(width().value(lengthContext), height().value(lengthContext));

    AffineTransform transform;
    if (!isOutermostSVG())
        transform.translate(x().value(lengthContext), y().value(lengthContext));
    else if (mode == SVGLocatable::ScreenScope) {
        if (RenderObject* renderer = this->renderer()) {
            // Translate in our CSS parent coordinate space
            // FIXME: This doesn't work correctly with CSS transforms.
            FloatPoint location = renderer->localToAbsolute(FloatPoint(), false, true);

            // Be careful here! localToAbsolute() includes the x/y offset coming from the viewBoxToViewTransform(), because
            // RenderSVGRoot::localToBorderBoxTransform() (called through mapLocalToContainer(), called from localToAbsolute())
            // also takes the viewBoxToViewTransform() into account, so we have to subtract it here (original cause of bug #27183)
            transform.translate(location.x() - viewBoxTransform.e(), location.y() - viewBoxTransform.f());

            // Respect scroll offset.
            if (FrameView* view = document()->view()) {
                LayoutSize scrollOffset = view->scrollOffset();
                transform.translate(-scrollOffset.width(), -scrollOffset.height());
            }
        }
    }

    return transform.multiply(viewBoxTransform);
}

RenderObject* SVGSVGElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    if (isOutermostSVG())
        return new (arena) RenderSVGRoot(this);

    return new (arena) RenderSVGViewportContainer(this);
}

void SVGSVGElement::insertedIntoDocument()
{
    document()->accessSVGExtensions()->addTimeContainer(this);
    SVGStyledLocatableElement::insertedIntoDocument();
}

void SVGSVGElement::removedFromDocument()
{
    document()->accessSVGExtensions()->removeTimeContainer(this);
    SVGStyledLocatableElement::removedFromDocument();
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

void SVGSVGElement::setCurrentTime(float /* seconds */)
{
    // FIXME: Implement me, bug 12073
}

bool SVGSVGElement::selfHasRelativeLengths() const
{
    return x().isRelative()
        || y().isRelative()
        || width().isRelative()
        || height().isRelative()
        || hasAttribute(SVGNames::viewBoxAttr);
}

bool SVGSVGElement::isOutermostSVG() const
{
    // Element may not be in the document, pretend we're outermost for viewport(), getCTM(), etc.
    if (!parentNode())
        return true;

    // We act like an outermost SVG element, if we're a direct child of a <foreignObject> element.
    if (parentNode()->hasTagName(SVGNames::foreignObjectTag))
        return true;

    // This is true whenever this is the outermost SVG, even if there are HTML elements outside it
    return !parentNode()->isSVGElement();
}

FloatRect SVGSVGElement::currentViewBoxRect(CalculateViewBoxMode mode) const
{
    // This method resolves length manually, w/o involving the render tree. This is desired, as getCTM()/getScreenCTM()/.. have to work without a renderer.
    SVGLengthContext lengthContext(this);

    // FIXME: The interaction of 'currentView' and embedding SVGs in other documents, is untested and unspecified.
    if (useCurrentView()) {
        if (SVGViewSpec* view = currentView()) // what if we should use it but it is not set?
            return view->viewBox();
        return FloatRect();
    }

    bool isEmbeddedThroughSVGImage = renderer() && renderer()->isSVGRoot() ? toRenderSVGRoot(renderer())->isEmbeddedThroughSVGImage() : false;
    bool hasFixedSize = width().unitType() != LengthTypePercentage && height().unitType() != LengthTypePercentage;

    FloatRect useViewBox = viewBox();
    if (useViewBox.isEmpty()) {
        // If no viewBox is specified but non-relative width/height values, then we
        // should always synthesize a viewBox if we're embedded through a SVGImage.
        if (hasFixedSize && isEmbeddedThroughSVGImage)
            return FloatRect(0, 0, width().value(lengthContext), height().value(lengthContext));
        return FloatRect();
    }

    // If a viewBox is specified and non-relative width/height values, then the host document only
    // uses the width/height values to figure out the intrinsic size when embedding us, whereas the
    // embedded document sees specified viewBox only.
    if (hasFixedSize && mode == CalculateViewBoxInHostDocument)
        return FloatRect(0, 0, width().value(lengthContext), height().value(lengthContext));

    return useViewBox;
}

AffineTransform SVGSVGElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    AffineTransform ctm = SVGFitToViewBox::viewBoxToViewTransform(currentViewBoxRect(), preserveAspectRatio(), viewWidth, viewHeight);
    if (useCurrentView() && currentView()) {
        AffineTransform transform;
        if (currentView()->transform().concatenate(transform))
            ctm *= transform;
    }

    return ctm;
}

void SVGSVGElement::setupInitialView(const String& fragmentIdentifier, Element* anchorNode)
{
    bool hadUseCurrentView = m_useCurrentView;
    setUseCurrentView(false);
    if (fragmentIdentifier.startsWith("xpointer(")) {
        // FIXME: XPointer references are ignored (https://bugs.webkit.org/show_bug.cgi?id=17491)
        return;
    }
    if (fragmentIdentifier.startsWith("svgView(")) {
        if (!currentView()->parseViewSpec(fragmentIdentifier))
            return;
        setUseCurrentView(true);
        return;
    }
    if (anchorNode && anchorNode->hasTagName(SVGNames::viewTag)) {
        SVGViewElement* viewElement = anchorNode->hasTagName(SVGNames::viewTag) ? static_cast<SVGViewElement*>(anchorNode) : 0;
        if (viewElement) {
            SVGElement* element = SVGLocatable::nearestViewportElement(viewElement);
            if (element->hasTagName(SVGNames::svgTag)) {
                SVGSVGElement* svg = static_cast<SVGSVGElement*>(element);
                svg->inheritViewAttributes(viewElement);
                setUseCurrentView(true);
            }
        }
        return;
    }
    if (hadUseCurrentView) {
        currentView()->setTransform(emptyString());
        if (RenderObject* object = renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(object);
    }
    // FIXME: We need to decide which <svg> to focus on, and zoom to it.
    // FIXME: We need to actually "highlight" the viewTarget(s).
}

void SVGSVGElement::inheritViewAttributes(SVGViewElement* viewElement)
{
    if (viewElement->hasAttribute(SVGNames::viewBoxAttr))
        currentView()->setViewBoxBaseValue(viewElement->viewBox());
    else
        currentView()->setViewBoxBaseValue(viewBox());

    SVGPreserveAspectRatio aspectRatio;
    if (viewElement->hasAttribute(SVGNames::preserveAspectRatioAttr))
        aspectRatio = viewElement->preserveAspectRatioBaseValue();
    else
        aspectRatio = preserveAspectRatioBaseValue();
    currentView()->setPreserveAspectRatioBaseValue(aspectRatio);

    if (viewElement->hasAttribute(SVGNames::zoomAndPanAttr))
        currentView()->setZoomAndPan(viewElement->zoomAndPan());
    
    if (RenderObject* object = renderer())
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(object);
}
    
void SVGSVGElement::documentWillSuspendForPageCache()
{
    pauseAnimations();
}

void SVGSVGElement::documentDidResumeFromPageCache()
{
    unpauseAnimations();
}

// getElementById on SVGSVGElement is restricted to only the child subtree defined by the <svg> element.
// See http://www.w3.org/TR/SVG11/struct.html#InterfaceSVGSVGElement
Element* SVGSVGElement::getElementById(const AtomicString& id) const
{
    Element* element = treeScope()->getElementById(id);
    if (element && element->isDescendantOf(this))
        return element;

    // Fall back to traversing our subtree. Duplicate ids are allowed, the first found will
    // be returned.
    for (Node* node = traverseNextNode(this); node; node = node->traverseNextNode(this)) {
        if (!node->isElementNode())
            continue;

        Element* element = static_cast<Element*>(node);
        if (element->hasID() && element->getIdAttribute() == id)
            return element;
    }
    return 0;
}

}

#endif // ENABLE(SVG)
