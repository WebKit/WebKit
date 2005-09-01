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

#include <qpoint.h>

#include <kdom/events/DocumentEventImpl.h>

#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGLocatableImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGZoomAndPanImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

namespace KDOM
{
    class DocumentPtr;
};

class KCanvasClipper;

namespace KSVG
{
    class SVGRectImpl;
    class SVGAngleImpl;
    class SVGPointImpl;
    class SVGNumberImpl;
    class SVGLengthImpl;
    class SVGMatrixImpl;
    class SVGTransformImpl;
    class SVGAnimatedLengthImpl;
    class SVGSVGElementImpl : public SVGStyledElementImpl,
                              public SVGTestsImpl,
                              public SVGLangSpaceImpl,
                              public SVGExternalResourcesRequiredImpl,
                              public SVGLocatableImpl,
                              public SVGFitToViewBoxImpl,
                              public SVGZoomAndPanImpl,
                              public KDOM::DocumentEventImpl
    {
    public:
        SVGSVGElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix);
        virtual ~SVGSVGElementImpl();

        // 'SVGSVGElement' functions
        SVGAnimatedLengthImpl *x() const;
        SVGAnimatedLengthImpl *y() const;
        SVGAnimatedLengthImpl *width() const;
        SVGAnimatedLengthImpl *height() const;

        KDOM::DOMStringImpl *contentScriptType() const;
        void setContentScriptType(KDOM::DOMStringImpl *type);

        KDOM::DOMStringImpl *contentStyleType() const;
        void setContentStyleType(KDOM::DOMStringImpl *type);

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

        void pauseAnimations();
        void unpauseAnimations();
        bool animationsPaused();

        float getCurrentTime();
        void setCurrentTime(float seconds);

        KDOM::NodeListImpl *getIntersectionList(SVGRectImpl *rect, SVGElementImpl *referenceElement);
        KDOM::NodeListImpl *getEnclosureList(SVGRectImpl *rect, SVGElementImpl *referenceElement);
        bool checkIntersection(SVGElementImpl *element, SVGRectImpl *rect);
        bool checkEnclosure(SVGElementImpl *element, SVGRectImpl *rect);
        void deselectAll();

        static SVGNumberImpl *createSVGNumber();
        static SVGLengthImpl *createSVGLength();
        static SVGAngleImpl *createSVGAngle();
        static SVGPointImpl *createSVGPoint(const QPoint &p = QPoint());
        static SVGMatrixImpl *createSVGMatrix();
        static SVGRectImpl *createSVGRect();
        static SVGTransformImpl *createSVGTransform();
        static SVGTransformImpl *createSVGTransformFromMatrix(SVGMatrixImpl *matrix);

        // 'DocumentEvent' functions
        virtual KDOM::EventImpl *createEvent(const KDOM::DOMString &eventType);

        virtual void parseAttribute(KDOM::AttributeImpl *);

        // 'virtual SVGLocatable' functions
        virtual SVGMatrixImpl *getCTM() const;
        virtual SVGMatrixImpl *getScreenCTM() const;

        virtual QString adjustViewportClipping() const;

        virtual bool implementsCanvasItem() const { return true; }
        virtual KCanvasItem *createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const;
        virtual void finalizeStyle(KCanvasRenderingStyle *style, bool needFillStrokeUpdate = true);

        // 'virtual SVGZoomAndPan functions
        virtual void setZoomAndPan(unsigned short zoomAndPan);

    private:
        mutable SVGAnimatedLengthImpl *m_x;
        mutable SVGAnimatedLengthImpl *m_y;
        mutable SVGAnimatedLengthImpl *m_width;
        mutable SVGAnimatedLengthImpl *m_height;

        bool m_useCurrentView;

        mutable KCanvasClipper *m_clipper;
    };
};

#endif

// vim:ts=4:noet
