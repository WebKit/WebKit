/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#pragma once

#include "FloatPoint.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGGraphicsElement.h"
#include "SVGZoomAndPan.h"

namespace WebCore {

class SMILTimeContainer;
class SVGAngle;
class SVGLength;
class SVGMatrix;
class SVGNumber;
class SVGRect;
class SVGTransform;
class SVGViewSpec;

class SVGSVGElement final : public SVGGraphicsElement, public SVGExternalResourcesRequired, public SVGFitToViewBox, public SVGZoomAndPan {

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGSVGElement)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_LENGTH(Width, width)
        DECLARE_ANIMATED_LENGTH(Height, height)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
        DECLARE_ANIMATED_RECT(ViewBox, viewBox)
        DECLARE_ANIMATED_PRESERVEASPECTRATIO(PreserveAspectRatio, preserveAspectRatio)
    END_DECLARE_ANIMATED_PROPERTIES

public: // DOM
    const AtomicString& contentScriptType() const;
    void setContentScriptType(const AtomicString&);

    const AtomicString& contentStyleType() const;
    void setContentStyleType(const AtomicString&);

    Ref<SVGRect> viewport() const;

    float pixelUnitToMillimeterX() const;
    float pixelUnitToMillimeterY() const;
    float screenPixelToMillimeterX() const;
    float screenPixelToMillimeterY() const;

    bool useCurrentView() const;
    SVGViewSpec& currentView();

    float currentScale() const;
    void setCurrentScale(float);

    Ref<SVGPoint> currentTranslate();
    FloatPoint currentTranslateValue();

    unsigned suspendRedraw(unsigned maxWaitMilliseconds);
    void unsuspendRedraw(unsigned suspendHandleId);
    void unsuspendRedrawAll();
    void forceRedraw();

    void pauseAnimations();
    void unpauseAnimations();
    bool animationsPaused() const;
    bool hasActiveAnimation() const;

    float getCurrentTime() const;
    void setCurrentTime(float);

    Ref<NodeList> getIntersectionList(SVGRect&, SVGElement* referenceElement);
    Ref<NodeList> getEnclosureList(SVGRect&, SVGElement* referenceElement);
    static bool checkIntersection(RefPtr<SVGElement>&&, SVGRect&);
    static bool checkEnclosure(RefPtr<SVGElement>&&, SVGRect&);
    void deselectAll();

    static Ref<SVGNumber> createSVGNumber();
    static Ref<SVGLength> createSVGLength();
    static Ref<SVGAngle> createSVGAngle();
    static Ref<SVGPoint> createSVGPoint();
    static Ref<SVGMatrix> createSVGMatrix();
    static Ref<SVGRect> createSVGRect();
    static Ref<SVGTransform> createSVGTransform();
    static Ref<SVGTransform> createSVGTransformFromMatrix(SVGMatrix&);

    Element* getElementById(const AtomicString&);

    SVGZoomAndPanType zoomAndPan() const;
    void setZoomAndPan(unsigned short);

public:
    static Ref<SVGSVGElement> create(const QualifiedName&, Document&);
    static Ref<SVGSVGElement> create(Document&);
    bool scrollToFragment(const String& fragmentIdentifier);
    void resetScrollAnchor();

    using SVGGraphicsElement::ref;
    using SVGGraphicsElement::deref;

    SMILTimeContainer& timeContainer();

    void setCurrentTranslate(const FloatPoint&); // Used to pan.
    void updateCurrentTranslate(); // Used from DOM bindings to create an SVGStaticPropertyTearOff for currentTranslate.

    bool hasIntrinsicWidth() const;
    bool hasIntrinsicHeight() const;
    Length intrinsicWidth() const;
    Length intrinsicHeight() const;

    FloatSize currentViewportSize() const;
    FloatRect currentViewBoxRect() const;

    bool hasEmptyViewBox() const;
    AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;

private:
    SVGSVGElement(const QualifiedName&, Document&);
    virtual ~SVGSVGElement();

    bool isValid() const override;
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) override;
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool rendererIsNeeded(const RenderStyle&) override;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void svgAttributeChanged(const QualifiedName&) override;
    bool selfHasRelativeLengths() const override;
    void prepareForDocumentSuspension() override;
    void resumeFromDocumentSuspension() override;
    AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const override;

    RefPtr<Frame> frameForCurrentScale() const;
    void inheritViewAttributes(const SVGViewElement&);
    Ref<NodeList> collectIntersectionOrEnclosureList(SVGRect&, SVGElement*, bool (*checkFunction)(SVGElement&, SVGRect&));

    SVGViewElement* findViewAnchor(const String& fragmentIdentifier) const;
    SVGSVGElement* findRootAnchor(const SVGViewElement*) const;
    SVGSVGElement* findRootAnchor(const String&) const;

    bool m_useCurrentView { false };
    SVGZoomAndPanType m_zoomAndPan { SVGZoomAndPanMagnify };
    Ref<SMILTimeContainer> m_timeContainer;
    FloatPoint m_currentTranslate;
    RefPtr<SVGViewSpec> m_viewSpec;
    String m_currentViewFragmentIdentifier;
};

inline bool SVGSVGElement::useCurrentView() const
{
    return m_useCurrentView;
}

inline FloatPoint SVGSVGElement::currentTranslateValue()
{
    return m_currentTranslate;
}

inline SVGZoomAndPanType SVGSVGElement::zoomAndPan() const
{
    return m_zoomAndPan;
}

inline void SVGSVGElement::setZoomAndPan(unsigned short zoomAndPan)
{
    m_zoomAndPan = parseFromNumber(zoomAndPan);
}

inline SMILTimeContainer& SVGSVGElement::timeContainer()
{
    return m_timeContainer.get();
}

inline bool SVGSVGElement::hasEmptyViewBox() const
{
    return viewBoxIsValid() && viewBox().isEmpty();
}

} // namespace WebCore
