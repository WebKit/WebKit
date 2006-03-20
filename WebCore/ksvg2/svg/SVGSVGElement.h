/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGSVGElementImpl_H
#define KSVG_SVGSVGElementImpl_H
#if SVG_SUPPORT

#include "IntPoint.h"

#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGFitToViewBox.h"
#include "SVGZoomAndPan.h"
#include "SVGStyledLocatableElement.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore
{
    class DocumentPtr;
};

namespace WebCore
{
    class SVGRect;
    class SVGAngle;
    class SVGPoint;
    class SVGNumber;
    class SVGLength;
    class SVGMatrix;
    class SVGTransform;
    class SVGAnimatedLength;
    class TimeScheduler;
    class SVGSVGElement : public SVGStyledLocatableElement,
                              public SVGTests,
                              public SVGLangSpace,
                              public SVGExternalResourcesRequired,
                              public SVGFitToViewBox,
                              public SVGZoomAndPan
    {
    public:
        SVGSVGElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGSVGElement();

        virtual bool isSVG() const { return true; }
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGSVGElement' functions
        SVGAnimatedLength *x() const;
        SVGAnimatedLength *y() const;
        SVGAnimatedLength *width() const;
        SVGAnimatedLength *height() const;

        AtomicString contentScriptType() const;
        void setContentScriptType(const AtomicString& type);

        AtomicString contentStyleType() const;
        void setContentStyleType(const AtomicString& type);

        SVGRect *viewport() const;

        float pixelUnitToMillimeterX() const;
        float pixelUnitToMillimeterY() const;
        float screenPixelToMillimeterX() const;
        float screenPixelToMillimeterY() const;

        bool useCurrentView() const;
        void setUseCurrentView(bool currentView);

        // SVGViewSpec *currentView() const;

        float currentScale() const;
        void setCurrentScale(float scale);

        SVGPoint *currentTranslate() const;

        unsigned long suspendRedraw(unsigned long max_wait_milliseconds);
        void unsuspendRedraw(unsigned long suspend_handle_id);
        void unsuspendRedrawAll();
        void forceRedraw();

        NodeList *getIntersectionList(SVGRect *rect, SVGElement *referenceElement);
        NodeList *getEnclosureList(SVGRect *rect, SVGElement *referenceElement);
        bool checkIntersection(SVGElement *element, SVGRect *rect);
        bool checkEnclosure(SVGElement *element, SVGRect *rect);
        void deselectAll();

        static SVGNumber *createSVGNumber();
        static SVGLength *createSVGLength();
        static SVGAngle *createSVGAngle();
        static SVGPoint *createSVGPoint(const IntPoint &p = IntPoint());
        static SVGMatrix *createSVGMatrix();
        static SVGRect *createSVGRect();
        static SVGTransform *createSVGTransform();
        static SVGTransform *createSVGTransformFromMatrix(SVGMatrix *matrix);

        virtual void parseMappedAttribute(MappedAttribute *attr);

        // 'virtual SVGLocatable' functions
        virtual SVGMatrix *getCTM() const;
        virtual SVGMatrix *getScreenCTM() const;

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);

        // 'virtual SVGZoomAndPan functions
        virtual void setZoomAndPan(unsigned short zoomAndPan);

    private:
        void addSVGWindowEventListner(const AtomicString& eventType, const Attribute* attr);   

        mutable RefPtr<SVGAnimatedLength> m_x;
        mutable RefPtr<SVGAnimatedLength> m_y;
        mutable RefPtr<SVGAnimatedLength> m_width;
        mutable RefPtr<SVGAnimatedLength> m_height;

        bool m_useCurrentView;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
