/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
                  2007 Apple Inc.  All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGSVGElement.h"

#include "AffineTransform.h"
#include "CSSHelper.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "RenderSVGViewportContainer.h"
#include "RenderSVGRoot.h"
#include "SVGAngle.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGViewElement.h"
#include "SVGViewSpec.h"
#include "SVGZoomEvent.h"
#include "SelectionController.h"
#include "SMILTimeContainer.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;
using namespace SVGNames;

SVGSVGElement::SVGSVGElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledLocatableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , SVGZoomAndPan()
    , m_x(this, SVGNames::xAttr, LengthModeWidth)
    , m_y(this, SVGNames::yAttr, LengthModeHeight)
    , m_width(this, SVGNames::widthAttr, LengthModeWidth, "100%")
    , m_height(this, SVGNames::heightAttr, LengthModeHeight, "100%")
    , m_useCurrentView(false)
    , m_timeContainer(SMILTimeContainer::create(this))
    , m_viewSpec(0)
    , m_containerSize(300, 150)
    , m_hasSetContainerSize(false)
{
    doc->registerForDocumentActivationCallbacks(this);
}

SVGSVGElement::~SVGSVGElement()
{
    document()->unregisterForDocumentActivationCallbacks(this);
    // There are cases where removedFromDocument() is not called.
    // see ContainerNode::removeAllChildren, called by its destructor.
    document()->accessSVGExtensions()->removeTimeContainer(this);

    // Call detach() here because if we wait until ~Node() calls it, we crash during
    // RenderSVGViewportContainer destruction, as the renderer assumes that the element
    // is still fully constructed. See <https://bugs.webkit.org/show_bug.cgi?id=21293>.
    if (renderer())
        detach();
}

const AtomicString& SVGSVGElement::contentScriptType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, defaultValue, ("text/ecmascript"));
    const AtomicString& n = getAttribute(contentScriptTypeAttr);
    return n.isNull() ? defaultValue : n;
}

void SVGSVGElement::setContentScriptType(const AtomicString& type)
{
    setAttribute(SVGNames::contentScriptTypeAttr, type);
}

const AtomicString& SVGSVGElement::contentStyleType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, defaultValue, ("text/css"));
    const AtomicString& n = getAttribute(contentStyleTypeAttr);
    return n.isNull() ? defaultValue : n;
}

void SVGSVGElement::setContentStyleType(const AtomicString& type)
{
    setAttribute(SVGNames::contentStyleTypeAttr, type);
}

FloatRect SVGSVGElement::viewport() const
{
    double _x = 0.0;
    double _y = 0.0;
    if (!isOutermostSVG()) {
        _x = x().value(this);
        _y = y().value(this);
    }
    float w = width().value(this);
    float h = height().value(this);
    AffineTransform viewBox = viewBoxToViewTransform(w, h);
    double wDouble = w;
    double hDouble = h;
    viewBox.map(_x, _y, &_x, &_y);
    viewBox.map(w, h, &wDouble, &hDouble);
    return FloatRect::narrowPrecision(_x, _y, wDouble, hDouble);
}

int SVGSVGElement::relativeWidthValue() const
{
    SVGLength w = width();
    if (w.unitType() != LengthTypePercentage)
        return 0;

    return static_cast<int>(w.valueAsPercentage() * m_containerSize.width());
}

int SVGSVGElement::relativeHeightValue() const
{
    SVGLength h = height();
    if (h.unitType() != LengthTypePercentage)
        return 0;

    return static_cast<int>(h.valueAsPercentage() * m_containerSize.height());
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
        m_viewSpec.set(new SVGViewSpec(this));

    return m_viewSpec.get();
}

float SVGSVGElement::currentScale() const
{
    if (document() && document()->frame())
        return document()->frame()->zoomFactor();
    return 1.0f;
}

void SVGSVGElement::setCurrentScale(float scale)
{
    if (document() && document()->frame())
        document()->frame()->setZoomFactor(scale, true);
}

FloatPoint SVGSVGElement::currentTranslate() const
{
    return m_translation;
}

void SVGSVGElement::setCurrentTranslate(const FloatPoint &translation)
{
    m_translation = translation;
    if (parentNode() == document() && document()->renderer())
        document()->renderer()->repaint();
}

void SVGSVGElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (!nearestViewportElement()) {
        bool setListener = true;

        // Only handle events if we're the outermost <svg> element
        if (attr->name() == onunloadAttr)
            document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().unloadEvent, attr);
        else if (attr->name() == onresizeAttr)
            document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().resizeEvent, attr);
        else if (attr->name() == onscrollAttr)
            document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().scrollEvent, attr);
        else if (attr->name() == SVGNames::onzoomAttr)
            document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().zoomEvent, attr);
        else
            setListener = false;
 
        if (setListener)
            return;
    }

    if (attr->name() == onabortAttr)
        document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().abortEvent, attr);
    else if (attr->name() == onerrorAttr)
        document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().errorEvent, attr);
    else if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::widthAttr) {
        setWidthBaseValue(SVGLength(LengthModeWidth, attr->value()));
        addCSSProperty(attr, CSSPropertyWidth, attr->value());
        if (widthBaseValue().value(this) < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for svg attribute <width> is not allowed");
    } else if (attr->name() == SVGNames::heightAttr) {
        setHeightBaseValue(SVGLength(LengthModeHeight, attr->value()));
        addCSSProperty(attr, CSSPropertyHeight, attr->value());
        if (heightBaseValue().value(this) < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for svg attribute <height> is not allowed");
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGFitToViewBox::parseMappedAttribute(attr))
            return;
        if (SVGZoomAndPan::parseMappedAttribute(attr))
            return;

        SVGStyledLocatableElement::parseMappedAttribute(attr);
    }
}

void SVGSVGElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledElement::svgAttributeChanged(attrName);

    if (!renderer())
        return;

    if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
        attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGFitToViewBox::isKnownAttribute(attrName) ||
        SVGZoomAndPan::isKnownAttribute(attrName) ||
        SVGStyledLocatableElement::isKnownAttribute(attrName))
        renderer()->setNeedsLayout(true);
}

unsigned long SVGSVGElement::suspendRedraw(unsigned long /* max_wait_milliseconds */)
{
    // FIXME: Implement me (see bug 11275)
    return 0;
}

void SVGSVGElement::unsuspendRedraw(unsigned long /* suspend_handle_id */, ExceptionCode&)
{
    // if suspend_handle_id is not found, throw exception
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

NodeList* SVGSVGElement::getIntersectionList(const FloatRect&, SVGElement*)
{
    // FIXME: Implement me (see bug 11274)
    return 0;
}

NodeList* SVGSVGElement::getEnclosureList(const FloatRect&, SVGElement*)
{
    // FIXME: Implement me (see bug 11274)
    return 0;
}

bool SVGSVGElement::checkIntersection(SVGElement*, const FloatRect& rect)
{
    // TODO : take into account pointer-events?
    // FIXME: Why is element ignored??
    // FIXME: Implement me (see bug 11274)
    return rect.intersects(getBBox());
}

bool SVGSVGElement::checkEnclosure(SVGElement*, const FloatRect& rect)
{
    // TODO : take into account pointer-events?
    // FIXME: Why is element ignored??
    // FIXME: Implement me (see bug 11274)
    return rect.contains(getBBox());
}

void SVGSVGElement::deselectAll()
{
    document()->frame()->selection()->clear();
}

float SVGSVGElement::createSVGNumber()
{
    return 0.0f;
}

SVGLength SVGSVGElement::createSVGLength()
{
    return SVGLength();
}

PassRefPtr<SVGAngle> SVGSVGElement::createSVGAngle()
{
    return SVGAngle::create();
}

FloatPoint SVGSVGElement::createSVGPoint()
{
    return FloatPoint();
}

AffineTransform SVGSVGElement::createSVGMatrix()
{
    return AffineTransform();
}

FloatRect SVGSVGElement::createSVGRect()
{
    return FloatRect();
}

SVGTransform SVGSVGElement::createSVGTransform()
{
    return SVGTransform();
}

SVGTransform SVGSVGElement::createSVGTransformFromMatrix(const AffineTransform& matrix)
{
    return SVGTransform(matrix);
}

AffineTransform SVGSVGElement::getCTM() const
{
    AffineTransform mat;
    if (!isOutermostSVG())
        mat.translate(x().value(this), y().value(this));

    if (attributes()->getNamedItem(SVGNames::viewBoxAttr)) {
        AffineTransform viewBox = viewBoxToViewTransform(width().value(this), height().value(this));
        mat = viewBox * mat;
    }

    return mat;
}

AffineTransform SVGSVGElement::getScreenCTM() const
{
    document()->updateLayoutIgnorePendingStylesheets();
    FloatPoint rootLocation;    

    if (RenderObject* renderer = this->renderer()) {
        if (isOutermostSVG()) {
            // FIXME: This doesn't work correctly with CSS transforms.
            FloatPoint point;
            if (renderer->parent())
                point = renderer->localToAbsolute(point, true);
            rootLocation.move(point.x(), point.y());
        } else
            rootLocation.move(x().value(this), y().value(this));
    }
    
    AffineTransform mat = SVGStyledLocatableElement::getScreenCTM();
    mat.translate(rootLocation.x(), rootLocation.y());

    if (attributes()->getNamedItem(SVGNames::viewBoxAttr)) {
        AffineTransform viewBox = viewBoxToViewTransform(width().value(this), height().value(this));
        mat = viewBox * mat;
    }

    return mat;
}

RenderObject* SVGSVGElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    if (isOutermostSVG())
        return new (arena) RenderSVGRoot(this);
    else
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

bool SVGSVGElement::hasRelativeValues() const
{
    return (x().isRelative() || width().isRelative() ||
            y().isRelative() || height().isRelative());
}

bool SVGSVGElement::isOutermostSVG() const
{
    // This is true whenever this is the outermost SVG, even if there are HTML elements outside it
    return !parentNode()->isSVGElement();
}

AffineTransform SVGSVGElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    FloatRect viewBoxRect;
    if (useCurrentView()) {
        if (currentView()) // what if we should use it but it is not set?
            viewBoxRect = currentView()->viewBox();
    } else
        viewBoxRect = viewBox();
    if (!viewBoxRect.width() || !viewBoxRect.height())
        return AffineTransform();

    AffineTransform ctm = preserveAspectRatio()->getCTM(viewBoxRect.x(),
            viewBoxRect.y(), viewBoxRect.width(), viewBoxRect.height(),
            0, 0, viewWidth, viewHeight);

    if (useCurrentView() && currentView())
        return currentView()->transform()->concatenate().matrix() * ctm;

    return ctm;
}

void SVGSVGElement::inheritViewAttributes(SVGViewElement* viewElement)
{
    setUseCurrentView(true);
    if (viewElement->hasAttribute(SVGNames::viewBoxAttr))
        currentView()->setViewBox(viewElement->viewBox());
    else
        currentView()->setViewBox(viewBox());
    if (viewElement->hasAttribute(SVGNames::preserveAspectRatioAttr)) {
        currentView()->preserveAspectRatio()->setAlign(viewElement->preserveAspectRatio()->align());
        currentView()->preserveAspectRatio()->setMeetOrSlice(viewElement->preserveAspectRatio()->meetOrSlice());
    } else {
        currentView()->preserveAspectRatio()->setAlign(preserveAspectRatio()->align());
        currentView()->preserveAspectRatio()->setMeetOrSlice(preserveAspectRatio()->meetOrSlice());
    }
    if (viewElement->hasAttribute(SVGNames::zoomAndPanAttr))
        currentView()->setZoomAndPan(viewElement->zoomAndPan());
    renderer()->setNeedsLayout(true);
}
    
void SVGSVGElement::documentWillBecomeInactive()
{
    pauseAnimations();
}

void SVGSVGElement::documentDidBecomeActive()
{
    unpauseAnimations();
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
