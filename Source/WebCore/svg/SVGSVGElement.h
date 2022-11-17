/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "SVGFitToViewBox.h"
#include "SVGGraphicsElement.h"
#include "SVGZoomAndPan.h"

namespace WebCore {

struct DOMMatrix2DInit;
class SMILTimeContainer;
class SVGAngle;
class SVGLength;
class SVGMatrix;
class SVGNumber;
class SVGRect;
class SVGTransform;
class SVGViewElement;
class SVGViewSpec;

class SVGSVGElement final : public SVGGraphicsElement, public SVGFitToViewBox, public SVGZoomAndPan {
    WTF_MAKE_ISO_ALLOCATED(SVGSVGElement);
public: // DOM
    float currentScale() const;
    void setCurrentScale(float);

    SVGPoint& currentTranslate() { return m_currentTranslate; }
    FloatPoint currentTranslateValue() const { return m_currentTranslate->value(); }

    bool useCurrentView() const { return m_useCurrentView; }
    SVGViewSpec& currentView();

    Ref<NodeList> getIntersectionList(SVGRect&, SVGElement* referenceElement);
    Ref<NodeList> getEnclosureList(SVGRect&, SVGElement* referenceElement);
    static bool checkIntersection(Ref<SVGElement>&&, SVGRect&);
    static bool checkEnclosure(Ref<SVGElement>&&, SVGRect&);

    void deselectAll();

    static Ref<SVGNumber> createSVGNumber();
    static Ref<SVGLength> createSVGLength();
    static Ref<SVGAngle> createSVGAngle();
    static Ref<SVGPoint> createSVGPoint();
    static Ref<SVGMatrix> createSVGMatrix();
    static Ref<SVGRect> createSVGRect();
    static Ref<SVGTransform> createSVGTransform();
    static Ref<SVGTransform> createSVGTransformFromMatrix(DOMMatrix2DInit&&);

    Element* getElementById(const AtomString&);

    void pauseAnimations();
    void unpauseAnimations();
    bool resumePausedAnimationsIfNeeded(const IntRect&);
    bool animationsPaused() const;
    bool hasActiveAnimation() const;
    float getCurrentTime() const;
    void setCurrentTime(float);
    
    unsigned suspendRedraw(unsigned) { return 0; }
    void unsuspendRedraw(unsigned) { }
    void unsuspendRedrawAll() { }
    void forceRedraw() { }

public:
    static Ref<SVGSVGElement> create(const QualifiedName&, Document&);
    static Ref<SVGSVGElement> create(Document&);
    bool scrollToFragment(StringView fragmentIdentifier);
    void resetScrollAnchor();

    using SVGGraphicsElement::ref;
    using SVGGraphicsElement::deref;

    SMILTimeContainer& timeContainer() { return m_timeContainer.get(); }

    void setCurrentTranslate(const FloatPoint&); // Used to pan.
    void updateCurrentTranslate();

    bool hasIntrinsicWidth() const;
    bool hasIntrinsicHeight() const;
    Length intrinsicWidth() const;
    Length intrinsicHeight() const;

    FloatSize currentViewportSizeExcludingZoom() const;
    FloatRect currentViewBoxRect() const;

    AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;
    bool hasTransformRelatedAttributes() const final;

    const SVGLengthValue& x() const { return m_x->currentValue(); }
    const SVGLengthValue& y() const { return m_y->currentValue(); }
    const SVGLengthValue& width() const { return m_width->currentValue(); }
    const SVGLengthValue& height() const { return m_height->currentValue(); }

    SVGAnimatedLength& xAnimated() { return m_x; }
    SVGAnimatedLength& yAnimated() { return m_y; }
    SVGAnimatedLength& widthAnimated() { return m_width; }
    SVGAnimatedLength& heightAnimated() { return m_height; }

    void inheritViewAttributes(const SVGViewElement&);

private:
    SVGSVGElement(const QualifiedName&, Document&);
    virtual ~SVGSVGElement();

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGSVGElement, SVGGraphicsElement, SVGFitToViewBox>;
    
    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;
    bool selfHasRelativeLengths() const override;
    bool isValid() const override;

    bool rendererIsNeeded(const RenderStyle&) override;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void prepareForDocumentSuspension() override;
    void resumeFromDocumentSuspension() override;
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) override;

    AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const override;
    RefPtr<Frame> frameForCurrentScale() const;
    Ref<NodeList> collectIntersectionOrEnclosureList(SVGRect&, SVGElement*, bool (*checkFunction)(SVGElement&, SVGRect&));

    SVGViewElement* findViewAnchor(StringView fragmentIdentifier) const;
    SVGSVGElement* findRootAnchor(const SVGViewElement*) const;
    SVGSVGElement* findRootAnchor(StringView) const;

    bool m_useCurrentView { false };
    Ref<SMILTimeContainer> m_timeContainer;
    RefPtr<SVGViewSpec> m_viewSpec;
    RefPtr<SVGViewElement> m_currentViewElement;
    String m_currentViewFragmentIdentifier;

    Ref<SVGPoint> m_currentTranslate { SVGPoint::create() };

    Ref<SVGAnimatedLength> m_x { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_y { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
    Ref<SVGAnimatedLength> m_width { SVGAnimatedLength::create(this, SVGLengthMode::Width, "100%"_s) };
    Ref<SVGAnimatedLength> m_height { SVGAnimatedLength::create(this, SVGLengthMode::Height, "100%"_s) };
};

} // namespace WebCore
