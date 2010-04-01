/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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

#ifndef SVGSVGElement_h
#define SVGSVGElement_h

#if ENABLE(SVG)

#include "IntSize.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGLangSpace.h"
#include "SVGStyledLocatableElement.h"
#include "SVGTests.h"
#include "SVGZoomAndPan.h"

namespace WebCore {
    class SVGAngle;
    class SVGLength;
    class SVGTransform;
    class SVGViewSpec;
    class SVGViewElement;
    class SMILTimeContainer;
    class SVGSVGElement : public SVGStyledLocatableElement,
                          public SVGTests,
                          public SVGLangSpace,
                          public SVGExternalResourcesRequired,
                          public SVGFitToViewBox,
                          public SVGZoomAndPan {
    public:
        SVGSVGElement(const QualifiedName&, Document*);
        virtual ~SVGSVGElement();

        virtual bool isSVG() const { return true; }
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGSVGElement' functions
        const AtomicString& contentScriptType() const;
        void setContentScriptType(const AtomicString& type);

        const AtomicString& contentStyleType() const;
        void setContentStyleType(const AtomicString& type);

        FloatRect viewport() const;

        void setContainerSize(const IntSize& containerSize) { m_containerSize = containerSize; m_hasSetContainerSize = true; }
        IntSize containerSize() const { return m_containerSize; }
        bool hasSetContainerSize() const { return m_hasSetContainerSize; }
        int relativeWidthValue() const;
        int relativeHeightValue() const;

        float pixelUnitToMillimeterX() const;
        float pixelUnitToMillimeterY() const;
        float screenPixelToMillimeterX() const;
        float screenPixelToMillimeterY() const;

        bool useCurrentView() const;
        void setUseCurrentView(bool currentView);

        SVGViewSpec* currentView() const;

        float currentScale() const;
        void setCurrentScale(float scale);

        FloatPoint currentTranslate() const;
        void setCurrentTranslate(const FloatPoint&);
        
        SMILTimeContainer* timeContainer() const { return m_timeContainer.get(); }
        
        void pauseAnimations();
        void unpauseAnimations();
        bool animationsPaused() const;

        float getCurrentTime() const;
        void setCurrentTime(float seconds);

        unsigned suspendRedraw(unsigned maxWaitMilliseconds);
        void unsuspendRedraw(unsigned suspendHandleId);
        void unsuspendRedrawAll();
        void forceRedraw();

        NodeList* getIntersectionList(const FloatRect&, SVGElement* referenceElement);
        NodeList* getEnclosureList(const FloatRect&, SVGElement* referenceElement);
        bool checkIntersection(SVGElement*, const FloatRect&);
        bool checkEnclosure(SVGElement*, const FloatRect&);
        void deselectAll();

        static float createSVGNumber();
        static SVGLength createSVGLength();
        static SVGAngle createSVGAngle();
        static FloatPoint createSVGPoint();
        static AffineTransform createSVGMatrix();
        static FloatRect createSVGRect();
        static SVGTransform createSVGTransform();
        static SVGTransform createSVGTransformFromMatrix(const AffineTransform&);

        virtual void parseMappedAttribute(MappedAttribute*);

        virtual bool rendererIsNeeded(RenderStyle* style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

        virtual void insertedIntoDocument();
        virtual void removedFromDocument();

        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void synchronizeProperty(const QualifiedName&);

        AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;

        void inheritViewAttributes(SVGViewElement*);

        bool isOutermostSVG() const;
        virtual bool hasRelativeValues() const;

    private:
        DECLARE_ANIMATED_PROPERTY(SVGSVGElement, SVGNames::xAttr, SVGLength, X, x)
        DECLARE_ANIMATED_PROPERTY(SVGSVGElement, SVGNames::yAttr, SVGLength, Y, y)
        DECLARE_ANIMATED_PROPERTY(SVGSVGElement, SVGNames::widthAttr, SVGLength, Width, width)
        DECLARE_ANIMATED_PROPERTY(SVGSVGElement, SVGNames::heightAttr, SVGLength, Height, height)

        // SVGExternalResourcesRequired
        DECLARE_ANIMATED_PROPERTY(SVGSVGElement, SVGNames::externalResourcesRequiredAttr, bool, ExternalResourcesRequired, externalResourcesRequired)

        // SVGFitToViewBox
        DECLARE_ANIMATED_PROPERTY(SVGSVGElement, SVGNames::viewBoxAttr, FloatRect, ViewBox, viewBox)
        DECLARE_ANIMATED_PROPERTY(SVGSVGElement, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio, PreserveAspectRatio, preserveAspectRatio)
 
        virtual void documentWillBecomeInactive();
        virtual void documentDidBecomeActive();

        virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const;

        bool m_useCurrentView;
        RefPtr<SMILTimeContainer> m_timeContainer;
        FloatPoint m_translation;
        float m_scale;
        mutable OwnPtr<SVGViewSpec> m_viewSpec;
        IntSize m_containerSize;
        bool m_hasSetContainerSize;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
