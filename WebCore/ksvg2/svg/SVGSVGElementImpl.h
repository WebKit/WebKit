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

#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGZoomAndPanImpl.h"
#include "SVGStyledLocatableElementImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

namespace WebCore
{
    class DocumentPtr;
};

namespace WebCore
{
    class SVGRectImpl;
    class SVGAngleImpl;
    class SVGPointImpl;
    class SVGNumberImpl;
    class SVGLengthImpl;
    class SVGMatrixImpl;
    class SVGTransformImpl;
    class SVGAnimatedLengthImpl;
    class TimeScheduler;
    class SVGSVGElementImpl : public SVGStyledLocatableElementImpl,
                              public SVGTestsImpl,
                              public SVGLangSpaceImpl,
                              public SVGExternalResourcesRequiredImpl,
                              public SVGFitToViewBoxImpl,
                              public SVGZoomAndPanImpl
    {
    public:
        SVGSVGElementImpl(const QualifiedName& tagName, DocumentImpl *doc);
        virtual ~SVGSVGElementImpl();

        virtual bool isSVG() const { return true; }
        
        virtual bool isValid() const { return SVGTestsImpl::isValid(); }

        // 'SVGSVGElement' functions
        SVGAnimatedLengthImpl *x() const;
        SVGAnimatedLengthImpl *y() const;
        SVGAnimatedLengthImpl *width() const;
        SVGAnimatedLengthImpl *height() const;

        AtomicString contentScriptType() const;
        void setContentScriptType(const AtomicString& type);

        AtomicString contentStyleType() const;
        void setContentStyleType(const AtomicString& type);

        SVGRectImpl *viewport() const;

        float pixelUnitToMillimeterX() const;
        float pixelUnitToMillimeterY() const;
        float screenPixelToMillimeterX() const;
        float screenPixelToMillimeterY() const;

        bool useCurrentView() const;
        void setUseCurrentView(bool currentView);

        // SVGViewSpecImpl *currentView() const;

        float currentScale() const;
        void setCurrentScale(float scale);

        SVGPointImpl *currentTranslate() const;

        unsigned long suspendRedraw(unsigned long max_wait_milliseconds);
        void unsuspendRedraw(unsigned long suspend_handle_id);
        void unsuspendRedrawAll();
        void forceRedraw();

        NodeListImpl *getIntersectionList(SVGRectImpl *rect, SVGElementImpl *referenceElement);
        NodeListImpl *getEnclosureList(SVGRectImpl *rect, SVGElementImpl *referenceElement);
        bool checkIntersection(SVGElementImpl *element, SVGRectImpl *rect);
        bool checkEnclosure(SVGElementImpl *element, SVGRectImpl *rect);
        void deselectAll();

        static SVGNumberImpl *createSVGNumber();
        static SVGLengthImpl *createSVGLength();
        static SVGAngleImpl *createSVGAngle();
        static SVGPointImpl *createSVGPoint(const IntPoint &p = IntPoint());
        static SVGMatrixImpl *createSVGMatrix();
        static SVGRectImpl *createSVGRect();
        static SVGTransformImpl *createSVGTransform();
        static SVGTransformImpl *createSVGTransformFromMatrix(SVGMatrixImpl *matrix);

        virtual void parseMappedAttribute(MappedAttributeImpl *attr);

        // 'virtual SVGLocatable' functions
        virtual SVGMatrixImpl *getCTM() const;
        virtual SVGMatrixImpl *getScreenCTM() const;

        virtual bool rendererIsNeeded(RenderStyle *) { return true; }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);

        // 'virtual SVGZoomAndPan functions
        virtual void setZoomAndPan(unsigned short zoomAndPan);

    private:
        void addSVGWindowEventListner(const AtomicString& eventType, const AttributeImpl* attr);   

        mutable RefPtr<SVGAnimatedLengthImpl> m_x;
        mutable RefPtr<SVGAnimatedLengthImpl> m_y;
        mutable RefPtr<SVGAnimatedLengthImpl> m_width;
        mutable RefPtr<SVGAnimatedLengthImpl> m_height;

        bool m_useCurrentView;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
